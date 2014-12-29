#include <string.h>
#include <assert.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "config.h"
#include "common/msg.h"
#include "common/av_common.h"
#include "options/options.h"
#include "ad.h"

#include "IL/OMX_Audio.h"

static OMX_AUDIO_CODINGTYPE mp_codec_to_omx_codec(const char* name)
{
    if (strcmp(name, "pcm_bluray") == 0 ||
        strcmp(name, "pcm_dvd") == 0 ||
        strcmp(name, "pcm_f32be") == 0 ||
        strcmp(name, "pcm_f32le") == 0 ||
        strcmp(name, "pcm_f64be") == 0 ||
        strcmp(name, "pcm_f64le") == 0 ||
        strcmp(name, "pcm_lxf") == 0 ||
        strcmp(name, "pcm_s8") == 0 ||
        strcmp(name, "pcm_s8_planar") == 0 ||
        strcmp(name, "pcm_s16be") == 0 ||
        strcmp(name, "pcm_s16le") == 0 ||
        strcmp(name, "pcm_s16le_planar") == 0 ||
        strcmp(name, "pcm_s24be") == 0 ||
        strcmp(name, "pcm_s24le") == 0 ||
        strcmp(name, "pcm_s32be") == 0 ||
        strcmp(name, "pcm_s32le") == 0 ||
        strcmp(name, "pcm_s32le_planar") == 0 ||
        strcmp(name, "pcm_u8") == 0 ||
        strcmp(name, "pcm_u16be") == 0 ||
        strcmp(name, "pcm_u16le") == 0 ||
        strcmp(name, "pcm_u24be") == 0 ||
        strcmp(name, "pcm_u24le") == 0 ||
        strcmp(name, "pcm_u32be") == 0 ||
        strcmp(name, "pcm_u32le") == 0)
    {
        return OMX_AUDIO_CodingPCM;
    }

    if (strcmp(name, "aac") == 0 ||
        strcmp(name, "aac_latm") == 0)
    {
        return OMX_AUDIO_CodingAAC;
    }

    if (strcmp(name, "mp3") == 0 ||
        strcmp(name, "mp3float") == 0)
    {
        return OMX_AUDIO_CodingMP3;
    }

    if (strcmp(name, "wmalossless") == 0 ||
        strcmp(name, "wmav1") == 0 ||
        strcmp(name, "wmav2") == 0 ||
        strcmp(name, "wmavoice") == 0)
    {
        return OMX_AUDIO_CodingWMA;
    }

    if (strcmp(name, "flac") == 0) {
        return OMX_AUDIO_CodingFLAC;
    }

    if (strcmp(name, "dts") == 0) {
        return OMX_AUDIO_CodingDTS;
    }

    if (strcmp(name, "wmapro") == 0) {
        return OMX_AUDIO_CodingWMAPRO;
    }

    if (strcmp(name, "atrac3") == 0 ||
        strcmp(name, "atrac3plus") == 0)
    {
        return OMX_AUDIO_CodingATRAC3;
    }

    return OMX_AUDIO_CodingMax;
}

static void mp_add_decoder_from_omx_codec_id(struct mp_decoder_list *list, OMX_AUDIO_CODINGTYPE id)
{
    switch (id) {
        case OMX_AUDIO_CodingPCM:
            mp_add_decoder(list, "rpi", "pcm_bluray", "pcm_bluray", "PCM signed 16|20|24-bit big-endian for Blu-ray media");
            mp_add_decoder(list, "rpi", "pcm_dvd", "pcm_dvd", "PCM signed 16|20|24-bit big-endian for DVD media");
            mp_add_decoder(list, "rpi", "pcm_f32be", "pcm_f32be", "PCM 32-bit floating point big-endian");
            mp_add_decoder(list, "rpi", "pcm_f32le", "pcm_f32le", "PCM 32-bit floating point little-endian");
            mp_add_decoder(list, "rpi", "pcm_f64be", "pcm_f64be", "PCM 64-bit floating point big-endian");
            mp_add_decoder(list, "rpi", "pcm_f64le", "pcm_f64le", "PCM 64-bit floating point little-endian");
            mp_add_decoder(list, "rpi", "pcm_lxf", "pcm_lxf", "PCM signed 20-bit little-endian planar");
            mp_add_decoder(list, "rpi", "pcm_s8", "pcm_s8", "PCM signed 8-bit");
            mp_add_decoder(list, "rpi", "pcm_s8_planar", "pcm_s8_planar", "PCM signed 8-bit planar");
            mp_add_decoder(list, "rpi", "pcm_s16be", "pcm_s16be", "PCM signed 16-bit big-endian");
            mp_add_decoder(list, "rpi", "pcm_s16be_planar", "pcm_s16be_planar", "PCM signed 16-bit big-endian planar");
            mp_add_decoder(list, "rpi", "pcm_s16le", "pcm_s16le", "PCM signed 16-bit little-endian");
            mp_add_decoder(list, "rpi", "pcm_s16le_planar", "pcm_s16le_planar", "PCM signed 16-bit little-endian planar");
            mp_add_decoder(list, "rpi", "pcm_s24be", "pcm_s24be", "PCM signed 24-bit big-endian");
            mp_add_decoder(list, "rpi", "pcm_s24le", "pcm_s24le", "PCM signed 24-bit little-endian");
            mp_add_decoder(list, "rpi", "pcm_s24le_planar", "pcm_s24le_planar", "PCM signed 24-bit little-endian planar");
            mp_add_decoder(list, "rpi", "pcm_s32be", "pcm_s32be", "PCM signed 32-bit big-endian");
            mp_add_decoder(list, "rpi", "pcm_s32le", "pcm_s32le", "PCM signed 32-bit little-endian");
            mp_add_decoder(list, "rpi", "pcm_s32le_planar", "pcm_s32le_planar", "PCM signed 32-bit little-endian planar");
            mp_add_decoder(list, "rpi", "pcm_u8", "pcm_u8", "PCM unsigned 8-bit");
            mp_add_decoder(list, "rpi", "pcm_u16be", "pcm_u16be", "PCM unsigned 16-bit big-endian");
            mp_add_decoder(list, "rpi", "pcm_u16le", "pcm_u16le", "PCM unsigned 16-bit little-endian");
            mp_add_decoder(list, "rpi", "pcm_u24be", "pcm_u24be", "PCM unsigned 24-bit big-endian");
            mp_add_decoder(list, "rpi", "pcm_u24le", "pcm_u24le", "PCM unsigned 24-bit little-endian");
            mp_add_decoder(list, "rpi", "pcm_u32be", "pcm_u32be", "PCM unsigned 32-bit big-endian");
            mp_add_decoder(list, "rpi", "pcm_u32le", "pcm_u32le", "PCM unsigned 32-bit little-endian");
            break;

        case OMX_AUDIO_CodingAAC:
            mp_add_decoder(list, "rpi", "aac", "aac", "AAC (Advanced Audio Coding)");
            mp_add_decoder(list, "rpi", "aac_latm", "aac_latm", "AAC LATM (Advanced Audio Coding LATM syntax)");
            break;

        case OMX_AUDIO_CodingMP3:
            mp_add_decoder(list, "rpi", "mp3", "mp3", "MP3 (MPEG audio layer 3)");
            mp_add_decoder(list, "rpi", "mp3float", "mp3float", "MP3 (MPEG audio layer 3)");
            break;

        case OMX_AUDIO_CodingWMA:
            mp_add_decoder(list, "rpi", "wmalossless", "wmalossless", "Windows Media Audio Lossless");
            mp_add_decoder(list, "rpi", "wmav1", "wmav1", "Windows Media Audio 1");
            mp_add_decoder(list, "rpi", "wmav2", "wmav2", "Windows Media Audio 2");
            mp_add_decoder(list, "rpi", "wmavoice", "wmavoice", "Windows Media Audio Voice");
            break;

        case OMX_AUDIO_CodingFLAC:
            mp_add_decoder(list, "rpi", "flac", "flac", "FLAC (Free Lossless Audio Codec)");
            break;

        case OMX_AUDIO_CodingDTS:
            mp_add_decoder(list, "rpi", "dts", "dts", "DTS");

        case OMX_AUDIO_CodingWMAPRO:
            mp_add_decoder(list, "rpi", "wmapro", "wmapro", "Windows Media Audio 9 Professional");
            break;

        case OMX_AUDIO_CodingATRAC3:
            mp_add_decoder(list, "rpi", "atrac3", "atrac3", "ATRAC3 (Adaptive TRansform Acoustic Coding 3)");
            mp_add_decoder(list, "rpi", "atrac3plus", "atrac3plus", "ATRAC3+ (Adaptive TRansform Acoustic Coding 3+)");
            break;

        case OMX_AUDIO_CodingUnused:
        case OMX_AUDIO_CodingAutoDetect:
        case OMX_AUDIO_CodingKhronosExtensions:
        case OMX_AUDIO_CodingVendorStartUnused:
        case OMX_AUDIO_CodingMax:
            break;
    }
}

static void add_decoders(struct mp_decoder_list *list)
{
    mp_add_decoder_from_omx_codec_id(list, OMX_AUDIO_CodingPCM);
    mp_add_decoder_from_omx_codec_id(list, OMX_AUDIO_CodingAAC);
    mp_add_decoder_from_omx_codec_id(list, OMX_AUDIO_CodingMP3);
    mp_add_decoder_from_omx_codec_id(list, OMX_AUDIO_CodingWMA);
    mp_add_decoder_from_omx_codec_id(list, OMX_AUDIO_CodingFLAC);
    mp_add_decoder_from_omx_codec_id(list, OMX_AUDIO_CodingDTS);
    mp_add_decoder_from_omx_codec_id(list, OMX_AUDIO_CodingWMAPRO);
    mp_add_decoder_from_omx_codec_id(list, OMX_AUDIO_CodingATRAC3);
}

static int init(struct dec_audio *da, const char *decoder)
{
    return 0;
}

static void uninit(struct dec_audio *da)
{
}

static int control(struct dec_audio *da, int cmd, void *arg)
{
    return CONTROL_UNKNOWN;
}

static int decode_packet(struct dec_audio *da, struct mp_audio **out)
{
    return AD_EOF;
}

const struct ad_functions ad_rpi = {
    .name = "rpi",
    .add_decoders = add_decoders,
    .init = init,
    .uninit = uninit,
    .control = control,
    .decode_packet = decode_packet,
};
