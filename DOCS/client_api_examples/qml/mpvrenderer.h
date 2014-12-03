#ifndef MPVRENDERER_H_
#define MPVRENDERER_H_

#include <assert.h>

#include <QtQuick/QQuickFramebufferObject>

#include "libmpv/client.h"
#include "libmpv/qthelper.hpp"

class MpvObject : public QQuickFramebufferObject
{
    Q_OBJECT

    mpv_handle *mpv;
    mpv_opengl_cb_context *mpv_gl;

public:
    MpvObject() {
        mpv = mpv_create();
        assert(mpv);
        mpv_set_option_string(mpv, "terminal", "yes");
        mpv_set_option_string(mpv, "msg-level", "all=v");
        mpv_initialize(mpv);
        mpv::qt::set_option_variant(mpv, "vo", "opengl-cb");
        mpv::qt::set_option_variant(mpv, "hwdec", "auto");
        mpv_gl = mpv_opengl_cb_get_context(mpv);
        assert(mpv_gl);
        mpv_opengl_cb_set_update_callback(mpv_gl, on_update, (void *)this);
        connect(this, &MpvObject::onUpdate, this, &MpvObject::doUpdate, Qt::QueuedConnection);
    }
    ~MpvObject() {
        mpv_terminate_destroy(mpv);
    }
    Renderer *createRenderer() const;
public slots:
    void loadfile() {
        QVariantList cmd;
        cmd.append("loadfile");
        cmd.append("../../../file.mkv");
        mpv::qt::command_variant(mpv, cmd);
    }
signals:
    void onUpdate();
private slots:
    // connected to onUpdate(); signal makes sure it runs on the GUI thread
    void doUpdate() {
        update();
    }
private:
    static void on_update(void *ctx) {
        MpvObject *self = (MpvObject *)ctx;
        emit self->onUpdate();
    }
};

#endif
