#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>

#include "config.h"

#include "talloc.h"
#include "common/common.h"
#include "misc/bstr.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "aspect.h"
#include "vo.h"
#include "video/vfcap.h"
#include "video/mp_image.h"
#include "sub/osd.h"

#include "common/global.h"
#include "player/client.h"

#include "gl_common.h"
#include "gl_video.h"
#include "gl_hwdec.h"

#include "video/decode/lavc.h" // HWDEC_* values

/*
 * mpv_opengl_cb_context is created by the host application - the host application
 * can access it any time, even if the VO is destroyed (or not created yet).
 * The OpenGL object allows initializing the renderer etc. The VO object is only
 * here to transfer the video frames somehow.
 */

struct vo_priv {
    struct vo *vo;

    struct mpv_opengl_cb_context *ctx;
};

struct mpv_opengl_cb_context {
    struct mp_log *log;

    pthread_mutex_t lock;

    // --- Protected by lock
    mpv_opengl_cb_update_fn update_cb;
    void *update_cb_ctx;
    struct mp_image *next_frame;
    struct mp_image_params img_params;
    struct mp_image_params *new_params;
    struct mp_rect wnd;
    bool force_update;
    bool imgfmt_supported[IMGFMT_END - IMGFMT_START];

    // --- All of these can only be accessed from the thread where the host
    //     application's OpenGL context is current - i.e. only while the
    //     host application is calling certain mpv_opengl_cb_* APIs.
    GL *gl;
    struct gl_video *renderer;
    struct gl_hwdec *hwdec;
    //struct gl_video_opts *renderer_opts;

    // --- Immutable or semi-threadsafe.

    struct osd_state *osd;
    struct mp_hwdec_info hwdec_info;
    const char *hwapi;

    struct vo *active;
};

struct mpv_opengl_cb_context *mp_opengl_create(struct mpv_global *g,
                                               struct osd_state *osd)
{
    mpv_opengl_cb_context *ctx = talloc_zero(NULL, mpv_opengl_cb_context);
    ctx->log = mp_log_new(ctx, g->log, "opengl-cb");
    pthread_mutex_init(&ctx->lock, NULL);

    // xxx
    //ctx->renderer_opts = m_config_alloc_struct(ctx, &gl_video_conf);

    ctx->gl = talloc_zero(ctx, GL);

    ctx->osd = osd;

    ctx->hwapi = "";
    // xxx this is pretty dumb
    switch (g->opts->hwdec_api) {
    case HWDEC_AUTO:    ctx->hwapi = "auto"; break;
    case HWDEC_VDPAU:   ctx->hwapi = "vdpau"; break;
    case HWDEC_VDA:     ctx->hwapi = "vda"; break;
    case HWDEC_VAAPI:   ctx->hwapi = "vaapi"; break;
    }

    return ctx;
}

typedef void (*mpv_opengl_update_fn)(void *cb_ctx);

void mpv_opengl_cb_set_update_callback(struct mpv_opengl_cb_context *ctx,
                                      mpv_opengl_update_fn callback,
                                      void *callback_ctx)
{
    pthread_mutex_lock(&ctx->lock);
    ctx->update_cb = callback;
    ctx->update_cb_ctx = callback_ctx;
    pthread_mutex_unlock(&ctx->lock);
}

typedef void *(*mpv_opengl_cb_get_proc_address_fn)(void *fn_ctx, const char *name);

int mpv_opengl_cb_init_gl(struct mpv_opengl_cb_context *ctx, const char *exts,
                          mpv_opengl_cb_get_proc_address_fn get_proc_address,
                          void *get_proc_address_ctx)
{
    assert(!ctx->renderer);

    mpgl_load_functions2(ctx->gl, get_proc_address, get_proc_address_ctx,
                         exts, ctx->log);
    int caps = MPGL_CAP_GL21 | MPGL_CAP_TEX_RG;
    if ((ctx->gl->mpgl_caps & caps) != caps) {
        MP_FATAL(ctx, "Missing OpenGL features.\n");
        return MPV_ERROR_INVALID_PARAMETER;
    }
    ctx->renderer = gl_video_init(ctx->gl, ctx->log, ctx->osd);
    ctx->hwdec = gl_hwdec_load_api(ctx->log, ctx->gl, ctx->hwapi, &ctx->hwdec_info);
    gl_video_set_hwdec(ctx->renderer, ctx->hwdec);

    pthread_mutex_lock(&ctx->lock);
    for (int n = IMGFMT_START; n < IMGFMT_END; n++) {
        ctx->imgfmt_supported[n - IMGFMT_START] =
            gl_video_check_format(ctx->renderer, n);
    }
    pthread_mutex_unlock(&ctx->lock);

    gl_video_unset_gl_state(ctx->renderer);
    return 0;
}

int mpv_opengl_cb_uninit_gl(struct mpv_opengl_cb_context *ctx)
{
    gl_video_uninit(ctx->renderer);
    ctx->renderer = NULL;
    gl_hwdec_uninit(ctx->hwdec);
    ctx->hwdec = NULL;
    talloc_free(ctx->gl);
    ctx->gl = NULL;
    return 0;
}

int mpv_opengl_cb_render(struct mpv_opengl_cb_context *ctx, int fbo, int vp[4])
{
    assert(ctx->renderer);

    gl_video_set_gl_state(ctx->renderer);

    pthread_mutex_lock(&ctx->lock);

    struct vo *vo = ctx->active;

    struct mp_image_params *new_params = ctx->new_params;
    ctx->new_params = NULL;
    if (new_params) {
        ctx->img_params = *new_params;
        ctx->force_update = true;
    }

    struct mp_rect wnd = {vp[0], vp[1], vp[0] + vp[2], vp[1] + vp[3]};
    if (wnd.x0 != ctx->wnd.x0 || wnd.y0 != ctx->wnd.y0 ||
        wnd.x1 != ctx->wnd.x1 || wnd.y1 != ctx->wnd.y1)
        ctx->force_update = true;

    if (ctx->force_update && vo) {
        ctx->force_update = false;
        ctx->wnd = wnd;

        struct mp_rect src, dst;
        struct mp_osd_res osd;
        // xxx: vo->opts is not synchronized
        mp_get_src_dst_rects(ctx->log, vo->opts, vo->driver->caps,
                             &ctx->img_params,
                             wnd.x1 - wnd.x0, wnd.y1 - wnd.y0,
                             1.0, &src, &dst, &osd);

        // compensate for flipped GL default coords
        if (fbo)
            MPSWAP(int, wnd.y0, wnd.y1);

        gl_video_resize(ctx->renderer, &wnd, &src, &dst, &osd);
    }

    if (new_params) {
        gl_video_config(ctx->renderer, new_params);
        talloc_free(new_params);
    }

    struct mp_image *mpi = ctx->next_frame;
    ctx->next_frame = NULL;

    pthread_mutex_unlock(&ctx->lock);

    if (mpi)
        gl_video_upload_image(ctx->renderer, mpi);

    gl_video_render_frame(ctx->renderer, fbo);

    gl_video_unset_gl_state(ctx->renderer);

    return 0;
}

static void draw_image(struct vo *vo, mp_image_t *mpi)
{
    struct vo_priv *p = vo->priv;
    if (p->ctx) {
        pthread_mutex_lock(&p->ctx->lock);
        mp_image_setrefp(&p->ctx->next_frame, mpi);
        pthread_mutex_unlock(&p->ctx->lock);
    }
    talloc_free(mpi);
}

static void flip_page(struct vo *vo)
{
    struct vo_priv *p = vo->priv;
    // It's really not clear how we should do this. Normally, vo_opengl renders
    // ahead by at most 50ms, then waits until supposed display time, and then
    // calls platform SwapBuffers.
    if (p->ctx) {
        pthread_mutex_lock(&p->ctx->lock);
        if (p->ctx->update_cb)
            p->ctx->update_cb(p->ctx->update_cb_ctx);
        pthread_mutex_unlock(&p->ctx->lock);
    }
}

static int query_format(struct vo *vo, uint32_t format)
{
    struct vo_priv *p = vo->priv;

    bool ok = false;
    if (p->ctx) {
        pthread_mutex_lock(&p->ctx->lock);
        if (format >= IMGFMT_START && format < IMGFMT_END)
            ok = p->ctx->imgfmt_supported[format - IMGFMT_START];
        pthread_mutex_unlock(&p->ctx->lock);
    }
    return ok ? VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW : 0;
}

static int reconfig(struct vo *vo, struct mp_image_params *params, int flags)
{
    struct vo_priv *p = vo->priv;

    if (p->ctx) {
        pthread_mutex_lock(&p->ctx->lock);
        mp_image_unrefp(&p->ctx->next_frame);
        talloc_free(p->ctx->new_params);
        p->ctx->new_params = talloc_memdup(NULL, params, sizeof(*params));
        pthread_mutex_unlock(&p->ctx->lock);
    } else {
        return -1;
    }

    return 0;
}

static int control(struct vo *vo, uint32_t request, void *data)
{
    struct vo_priv *p = vo->priv;

    switch (request) {
    case VOCTRL_SET_LIBMPV_OPENGL_CB_CONTEXT: {
        if (p->ctx)
            return VO_FALSE;
        struct mpv_opengl_cb_context *nctx = data;
        if (nctx) {
            pthread_mutex_lock(&nctx->lock);
            if (nctx->active) {
                MP_FATAL(vo, "There is already a VO using the OpenGL context.\n");
            } else {
                nctx->active = vo;
                p->ctx = nctx;
                assert(vo->osd == p->ctx->osd);
            }
            pthread_mutex_unlock(&nctx->lock);
        }
        return VO_TRUE;
    }
    case VOCTRL_GET_PANSCAN:
        return VO_TRUE;
    case VOCTRL_SET_PANSCAN:
    case VOCTRL_REDRAW_FRAME:
        pthread_mutex_lock(&p->ctx->lock);
        p->ctx->force_update = true;
        if (p->ctx->update_cb)
            p->ctx->update_cb(p->ctx->update_cb_ctx);
        pthread_mutex_unlock(&p->ctx->lock);
        return VO_TRUE;
    case VOCTRL_GET_HWDEC_INFO: {
        // xxx: in theory, the API user could destroy the OpenGL context while
        //      the decoder uses the hwdec thing, and bad things would happen.
        struct mp_hwdec_info **arg = data;
        *arg = p->ctx ? &p->ctx->hwdec_info : NULL;
        return true;
    }
    }

    return VO_NOTIMPL;
}

static void uninit(struct vo *vo)
{
    struct vo_priv *p = vo->priv;

    if (p->ctx) {
        pthread_mutex_lock(&p->ctx->lock);
        mp_image_unrefp(&p->ctx->next_frame);
        talloc_free(p->ctx->new_params);
        p->ctx->new_params = NULL;
        p->ctx->active = NULL;
        pthread_mutex_unlock(&p->ctx->lock);
    }
}

static int preinit(struct vo *vo)
{
    struct vo_priv *p = vo->priv;
    p->vo = vo;
    return 0;
}

const struct vo_driver video_out_opengl_cb = {
    .description = "OpenGL Callbacks for libmpv",
    .name = "opengl-cb",
    .caps = VO_CAP_ROTATE90,
    .preinit = preinit,
    .query_format = query_format,
    .reconfig = reconfig,
    .control = control,
    .draw_image = draw_image,
    .flip_page = flip_page,
    .uninit = uninit,
    .priv_size = sizeof(struct vo_priv),
    //.options = options,
};
