#include "mpvrenderer.h"

#include <QObject>
#include <QtGlobal>
#include <QOpenGLContext>

#include <QtGui/QOpenGLFramebufferObject>

#include <QtQuick/QQuickWindow>
#include <qsgsimpletexturenode.h>

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
    static void *get_proc_address(void *ctx, const char *name) {
        (void)ctx;
        QOpenGLContext *glctx = QOpenGLContext::currentContext();
        if (!glctx)
            return NULL;
        return (void *)glctx->getProcAddress(QByteArray(name));
    }

    mpv_opengl_cb_context *mpv_gl;
    QQuickWindow *window;
public:
    MpvRenderer(mpv_opengl_cb_context *a_mpv_gl) : mpv_gl(a_mpv_gl), window(0) {
        int r = mpv_opengl_cb_init_gl(mpv_gl, "", get_proc_address, NULL);
        assert(r >= 0);
    }

    void render() {
        assert(window);

        QOpenGLFramebufferObject *fbo = framebufferObject();
        int vp[4] = {0, 0, fbo->width(), fbo->height()};
        window->resetOpenGLState();
        mpv_opengl_cb_render(mpv_gl, fbo->handle(), vp);
        window->resetOpenGLState();
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) {
        QOpenGLFramebufferObjectFormat format;
        format.setSamples(4);
        return new QOpenGLFramebufferObject(size, format);
    }

protected:
    virtual void synchronize(QQuickFramebufferObject *item) {
        window = item->window();
    }
};

QQuickFramebufferObject::Renderer *MpvObject::createRenderer() const
{
    window()->setPersistentOpenGLContext(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(mpv_gl);
}
