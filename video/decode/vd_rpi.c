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

#include "IL/OMX_Video.h"

static OMX_VIDEO_CODINGTYPE mp_codec_to_omx_codec(const char* name)
{
    if (strcmp(name, "mpeg2video") == 0) {
        return OMX_VIDEO_CodingMPEG2;
    }

    if (strcmp(name, "h263") == 0) {
        return OMX_VIDEO_CodingH263;
    }

    if (strcmp(name, "mpeg4") == 0) {
        return OMX_VIDEO_CodingMPEG4;
    }

    if (strcmp(name, "wmv1") == 0 ||
        strcmp(name, "wmv2") == 0 ||
        strcmp(name, "wmv3") == 0)
    {
        return OMX_VIDEO_CodingWMV;
    }

    if (strcmp(name, "rv10") == 0 ||
        strcmp(name, "rv20") == 0 ||
        strcmp(name, "rv30") == 0 ||
        strcmp(name, "rv40") == 0)
    {
        return OMX_VIDEO_CodingRV;
    }

    if (strcmp(name, "h264") == 0) {
        return OMX_VIDEO_CodingAVC;
    }

    if (strcmp(name, "mjpeg") == 0) {
        return OMX_VIDEO_CodingMJPEG;
    }

    if (strcmp(name, "vp6") == 0) {
        return OMX_VIDEO_CodingVP6;
    }

    if (strcmp(name, "vp7") == 0) {
        return OMX_VIDEO_CodingVP7;
    }

    if (strcmp(name, "vp8") == 0) {
        return OMX_VIDEO_CodingVP8;
    }

    if (strcmp(name, "y41p") == 0) {
        return OMX_VIDEO_CodingYUV;
    }

    if (strcmp(name, "svq1") == 0 ||
        strcmp(name, "svq3") == 0)
    {
        return OMX_VIDEO_CodingSorenson;
    }

    if (strcmp(name, "theora") == 0) {
        return OMX_VIDEO_CodingTheora;
    }

    return OMX_VIDEO_CodingMax;
}

static void mp_add_decoder_from_omx_codec_id(struct mp_decoder_list *list, OMX_VIDEO_CODINGTYPE id)
{
    switch (id) {
        case OMX_VIDEO_CodingMPEG2:
            mp_add_decoder(list, "rpi", "mpeg2video", "mpeg2video", "MPEG-2 video");
            break;

        case OMX_VIDEO_CodingH263:
            mp_add_decoder(list, "rpi", "h263", "h263", "H.263 / H.263-1996, H.263+ / H.263-1998 / H.263 version 2");
            break;

        case OMX_VIDEO_CodingMPEG4:
            mp_add_decoder(list, "rpi", "mpeg4", "mpeg4", "MPEG-4 part 2");
            break;

        case OMX_VIDEO_CodingWMV:
            mp_add_decoder(list, "rpi", "wmv1", "wmv1", "Windows Media Video 7");
            mp_add_decoder(list, "rpi", "wmv2", "wmv2", "Windows Media Video 8");
            mp_add_decoder(list, "rpi", "wmv3", "wmv3", "Windows Media Video 9");
            break;

        case OMX_VIDEO_CodingRV:
            mp_add_decoder(list, "rpi", "rv10", "rv10", "RealVideo 1.0");
            mp_add_decoder(list, "rpi", "rv20", "rv20", "RealVideo 2.0");
            mp_add_decoder(list, "rpi", "rv30", "rv30", "RealVideo 3.0");
            mp_add_decoder(list, "rpi", "rv40", "rv40", "RealVideo 4.0");
            break;

        case OMX_VIDEO_CodingAVC:
            mp_add_decoder(list, "rpi", "h264", "h264", "H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10");
            break;

        case OMX_VIDEO_CodingMJPEG:
            mp_add_decoder(list, "rpi", "mjpeg", "mjpeg", "MJPEG (Motion JPEG)");
            break;

        case OMX_VIDEO_CodingVP6:
            mp_add_decoder(list, "rpi", "vp6", "vp6", "On2 VP6");
            break;

        case OMX_VIDEO_CodingVP7:
            mp_add_decoder(list, "rpi", "vp7", "vp7", "On2 VP7");
            break;

        case OMX_VIDEO_CodingVP8:
            mp_add_decoder(list, "rpi", "vp8", "vp8", "On2 VP8");
            break;

        case OMX_VIDEO_CodingYUV:
            mp_add_decoder(list, "rpi", "y41p", "y41p", "Uncompressed YUV 4:1:1 12-bit");
            break;

        case OMX_VIDEO_CodingSorenson:
            mp_add_decoder(list, "rpi", "svq1", "svq1", "Sorenson Vector Quantizer 1 / Sorenson Video 1 / SVQ1");
            mp_add_decoder(list, "rpi", "svq3", "svq3", "Sorenson Vector Quantizer 3 / Sorenson Video 3 / SVQ3");
            break;

        case OMX_VIDEO_CodingTheora:
            mp_add_decoder(list, "rpi", "theora", "theora", "Theora");
            break;

        case OMX_VIDEO_CodingMVC:
            // NOTE: comments say this is H.264/MVC, what's that?
            break;

        case OMX_VIDEO_CodingUnused:
        case OMX_VIDEO_CodingAutoDetect:
        case OMX_VIDEO_CodingKhronosExtensions:
        case OMX_VIDEO_CodingVendorStartUnused:
        case OMX_VIDEO_CodingMax:
            break;
    }
}

static void add_decoders(struct mp_decoder_list *list)
{
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingMPEG2);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingH263);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingMPEG4);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingWMV);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingRV);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingAVC);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingMJPEG);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingVP6);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingVP7);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingVP8);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingYUV);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingSorenson);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingTheora);
    mp_add_decoder_from_omx_codec_id(list, OMX_VIDEO_CodingMVC);
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
