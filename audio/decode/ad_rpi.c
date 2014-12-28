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

static void add_decoders(struct mp_decoder_list *list)
{
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
