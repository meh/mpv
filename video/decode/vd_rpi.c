#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>

#include "talloc.h"
#include "config.h"
#include "common/msg.h"
#include "options/options.h"
#include "misc/bstr.h"
#include "common/av_common.h"
#include "common/codecs.h"

#include "video/fmt-conversion.h"

#include "vd.h"
#include "video/img_format.h"
#include "video/filter/vf.h"
#include "video/decode/dec_video.h"
#include "demux/stheader.h"
#include "demux/packet.h"
#include "video/csputils.h"

static void add_decoders(struct mp_decoder_list *list)
{
}

static int init(struct dec_video *vd, const char *decoder)
{
    return 0;
}

static void uninit(struct dec_video *vd)
{
}

static int control(struct dec_video *vd, int cmd, void *arg)
{
    return 0;
}

static struct mp_image *decode(struct dec_video *vd, struct demux_packet *packet, int flags)
{
    return NULL;
}

const struct vd_functions vd_rpi = {
    .name = "rpi",
    .add_decoders = add_decoders,
    .init = init,
    .uninit = uninit,
    .control = control,
    .decode = decode,
};
