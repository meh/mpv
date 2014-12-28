/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <libavutil/mem.h>

#include "demux/codec_tags.h"

#include "config.h"
#include "common/codecs.h"
#include "common/msg.h"
#include "misc/bstr.h"

#include "stream/stream.h"
#include "demux/demux.h"

#include "demux/stheader.h"

#include "dec_audio.h"
#include "ad.h"
#include "audio/format.h"
#include "audio/audio.h"
#include "audio/audio_buffer.h"

#include "audio/filter/af.h"

extern const struct ad_functions ad_mpg123;
extern const struct ad_functions ad_lavc;
extern const struct ad_functions ad_spdif;
extern const struct ad_functions ad_rpi;

static const struct ad_functions * const ad_drivers[] = {
    &ad_lavc,
#if HAVE_MPG123
    &ad_mpg123,
#endif
#if HAVE_RPI
    &ad_rpi,
#endif
    &ad_spdif,
    NULL
};

static void uninit_decoder(struct dec_audio *d_audio)
{
    if (d_audio->ad_driver) {
        MP_VERBOSE(d_audio, "Uninit audio decoder.\n");
        d_audio->ad_driver->uninit(d_audio);
    }
    d_audio->ad_driver = NULL;
    talloc_free(d_audio->priv);
    d_audio->priv = NULL;
}

static int init_audio_codec(struct dec_audio *d_audio, const char *decoder)
{
    if (!d_audio->ad_driver->init(d_audio, decoder)) {
        MP_VERBOSE(d_audio, "Audio decoder init failed.\n");
        d_audio->ad_driver = NULL;
        uninit_decoder(d_audio);
        return 0;
    }

    return 1;
}

struct mp_decoder_list *audio_decoder_list(void)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    for (int i = 0; ad_drivers[i] != NULL; i++)
        ad_drivers[i]->add_decoders(list);
    return list;
}

static struct mp_decoder_list *audio_select_decoders(const char *codec,
                                                     char *selection)
{
    struct mp_decoder_list *list = audio_decoder_list();
    struct mp_decoder_list *new = mp_select_decoders(list, codec, selection);
    talloc_free(list);
    return new;
}

static const struct ad_functions *find_driver(const char *name)
{
    for (int i = 0; ad_drivers[i] != NULL; i++) {
        if (strcmp(ad_drivers[i]->name, name) == 0)
            return ad_drivers[i];
    }
    return NULL;
}

int audio_init_best_codec(struct dec_audio *d_audio, char *audio_decoders)
{
    assert(!d_audio->ad_driver);
    audio_reset_decoding(d_audio);

    struct mp_decoder_entry *decoder = NULL;
    struct mp_decoder_list *list =
        audio_select_decoders(d_audio->header->codec, audio_decoders);

    mp_print_decoders(d_audio->log, MSGL_V, "Codec list:", list);

    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *sel = &list->entries[n];
        const struct ad_functions *driver = find_driver(sel->family);
        if (!driver)
            continue;
        MP_VERBOSE(d_audio, "Opening audio decoder %s:%s\n",
                   sel->family, sel->decoder);
        d_audio->ad_driver = driver;
        if (init_audio_codec(d_audio, sel->decoder)) {
            decoder = sel;
            break;
        }
        MP_WARN(d_audio, "Audio decoder init failed for "
                "%s:%s\n", sel->family, sel->decoder);
    }

    if (d_audio->ad_driver) {
        d_audio->decoder_desc =
            talloc_asprintf(d_audio, "%s [%s:%s]", decoder->desc, decoder->family,
                            decoder->decoder);
        MP_VERBOSE(d_audio, "Selected audio codec: %s\n", d_audio->decoder_desc);
    } else {
        MP_ERR(d_audio, "Failed to initialize an audio decoder for codec '%s'.\n",
               d_audio->header->codec ? d_audio->header->codec : "<unknown>");
    }

    talloc_free(list);
    return !!d_audio->ad_driver;
}

void audio_uninit(struct dec_audio *d_audio)
{
    if (!d_audio)
        return;
    MP_VERBOSE(d_audio, "Uninit audio filters...\n");
    af_destroy(d_audio->afilter);
    uninit_decoder(d_audio);
    talloc_free(d_audio);
}

/* Decode packets until we know the audio format. Then reinit the buffer.
 * Returns AD_OK on success, negative AD_* code otherwise.
 * Also returns AD_OK if already initialized (and does nothing).
 */
int initial_audio_decode(struct dec_audio *da)
{
    while (!da->waiting) {
        int ret = da->ad_driver->decode_packet(da, &da->waiting);
        if (ret < 0)
            return ret;
    }
    talloc_steal(da, da->waiting);
    da->decode_format = *da->waiting;
    return mp_audio_config_valid(da->waiting) ? AD_OK : AD_ERR;
}

/* Try to get at least minsamples decoded+filtered samples in outbuf
 * (total length including possible existing data).
 * Return 0 on success, or negative AD_* error code.
 * In the former case outbuf has at least minsamples buffered on return.
 * In case of EOF/error it might or might not be. */
int audio_decode(struct dec_audio *da, struct mp_audio_buffer *outbuf,
                 int minsamples)
{
    struct af_stream *afs = da->afilter;
    if (afs->initialized < 1)
        return AD_ERR;

    MP_STATS(da, "start audio");

    int res = 0;
    while (res >= 0 && minsamples >= 0) {
        int buffered = mp_audio_buffer_samples(outbuf);
        if (minsamples < buffered)
            break;

        res = 0;

        struct mp_audio *mpa = da->waiting;
        if (!mpa)
            res = da->ad_driver->decode_packet(da, &mpa);

        if (res != AD_EOF) {
            if (res < 0)
                break;
            if (!mpa )
                continue;
        }

        if (mpa) {
            da->pts_offset += mpa->samples;
            da->decode_format = *mpa;
            mp_audio_set_null_data(&da->decode_format);
            // On format change, make sure to drain the filter chain.
            if (!mp_audio_config_equals(&afs->input, mpa)) {
                res = AD_NEW_FMT;
                da->waiting = talloc_steal(da, mpa);
                mpa = NULL;
            }
        }

        if (mpa)
            da->waiting = NULL;

        if (af_filter(afs, mpa, outbuf) < 0)
            return AD_ERR;
    }

    MP_STATS(da, "end audio");

    return res;
}

void audio_reset_decoding(struct dec_audio *d_audio)
{
    if (d_audio->ad_driver)
        d_audio->ad_driver->control(d_audio, ADCTRL_RESET, NULL);
    af_control_all(d_audio->afilter, AF_CONTROL_RESET, NULL);
    d_audio->pts = MP_NOPTS_VALUE;
    d_audio->pts_offset = 0;
    if (d_audio->waiting) {
        talloc_free(d_audio->waiting);
        d_audio->waiting = NULL;
    }
}
