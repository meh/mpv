#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "config.h"
#include "vo.h"
#include "video/mp_image.h"
#include "video/vfcap.h"
#include "video/memcpy_pic.h"

#include "input/keycodes.h"
#include "input/input.h"
#include "common/msg.h"
#include "input/input.h"

struct priv {
    int screen_w, screen_h;
};

static int preinit(struct vo *vo)
{
    return 0;
}

static int query_format(struct vo *vo, uint32_t format)
{
    return 0;
}

static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    return VO_NOTIMPL;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
}

static void flip_page(struct vo *vo)
{
}

static void uninit(struct vo *vo)
{
}

const struct vo_driver video_out_rpi = {
    .description = "Raspberry Pi Renderer",
    .name = "rpi",
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct priv),
};
