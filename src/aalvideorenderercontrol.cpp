/*
 * Copyright (C) 2012 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "aalvideorenderercontrol.h"
#include "aalcameraservice.h"
#include "aalviewfindersettingscontrol.h"

#include <hybris/camera/camera_compatibility_layer.h>
#include <hybris/camera/camera_compatibility_layer_capabilities.h>
#include <qtubuntu_media_signals.h>

#include <QAbstractVideoBuffer>
#include <QAbstractVideoSurface>
#include <QDebug>
#include <QTimer>
#include <QUrl>
#include <QVideoSurfaceFormat>

class AalGLTextureBuffer : public QAbstractVideoBuffer
{
public:
    AalGLTextureBuffer(GLuint textureId, int width, int height) :
        QAbstractVideoBuffer(QAbstractVideoBuffer::GLTextureHandle),
        m_textureId(textureId),
        m_mapMode(NotMapped),
        m_width(width),
        m_height(height)
    {
    }

    MapMode mapMode() const { return m_mapMode; }
    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine)
    {
        if (mode != MapMode::ReadOnly) {
            qWarning() << "Tried to map in invalid mode:" << mode;
            return (uchar*)0;
        }

        const int width = m_width;
        const int height = m_height;

        if (m_pixelBuffer)
            delete[] m_pixelBuffer;
        glBindTexture(GL_TEXTURE_2D, m_textureId);

        m_pixelBuffer = new uint8_t[width * height * 4];

        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, m_pixelBuffer);

        m_mapMode = mode;
        *numBytes = width * height * 4;
        *bytesPerLine = width * 4;
        return (uchar*)m_pixelBuffer;
    }

    void unmap()
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        if (m_pixelBuffer)
            delete[] m_pixelBuffer;
    }

    QVariant handle() const
    {
        return QVariant::fromValue<unsigned int>(m_textureId);
    }

    GLuint textureId() { return m_textureId; }

private:
    GLuint m_textureId;
    int m_width;
    int m_height;
    MapMode m_mapMode;
    uint8_t* m_pixelBuffer = nullptr;
};


AalVideoRendererControl::AalVideoRendererControl(AalCameraService *service, QObject *parent)
   : QVideoRendererControl(parent)
   , m_surface(0),
     m_service(service),
     m_viewFinderRunning(false),
     m_previewStarted(false),
     m_textureId(0)
{
    // Get notified when qtvideo-node creates a GL texture
    connect(SharedSignal::instance(), SIGNAL(textureCreated(unsigned int)), this, SLOT(onTextureCreated(unsigned int)));
    connect(SharedSignal::instance(), SIGNAL(snapshotTaken(QImage)), this, SLOT(onSnapshotTaken(QImage)));
}

AalVideoRendererControl::~AalVideoRendererControl()
{
}

QAbstractVideoSurface *AalVideoRendererControl::surface() const
{
    return m_surface;
}

void AalVideoRendererControl::setSurface(QAbstractVideoSurface *surface)
{
    if (m_surface != surface) {
        m_surface = surface;
        Q_EMIT surfaceChanged(surface);
    }
}

void AalVideoRendererControl::init(CameraControl *control, CameraControlListener *listener)
{
    Q_UNUSED(control);
    listener->on_preview_texture_needs_update_cb = &AalVideoRendererControl::updateViewfinderFrameCB;
    // ensures a new texture will be created by qtvideo-node
    m_textureId = 0;
}

void AalVideoRendererControl::startPreview()
{
    if (m_previewStarted) {
        return;
    }
    if (!m_service->androidControl()) {
        qWarning() << "Can't start preview without a CameraControl";
        return;
    }
    m_previewStarted = true;

    if (m_textureId) {
        CameraControl *cc = m_service->androidControl();
        android_camera_set_preview_texture(cc, m_textureId);
        android_camera_start_preview(cc);
    }

    // if no texture ID is set to the frame passed to ShaderVideoNode,
    // a texture ID will be generated and returned via the 'textureCreated' signal
    // after calling updateViewfinderFrame()
    updateViewfinderFrame();

    m_service->updateCaptureReady();
}

void AalVideoRendererControl::stopPreview()
{
    if (!m_previewStarted) {
        return;
    }
    if (!m_service->androidControl()) {
        qWarning() << "Can't stop preview without a CameraControl";
        return;
    }
    if (!m_surface) {
        qWarning() << "Can't stop preview without a QAbstractVideoSurface";
        return;
    }

    if (m_surface->isActive()) {
        m_surface->stop();
    }

    CameraControl *cc = m_service->androidControl();
    android_camera_stop_preview(cc);
    // FIXME: missing android_camera_set_preview_size(QSize())
    android_camera_set_preview_texture(cc, 0);

    m_previewStarted = false;
    m_service->updateCaptureReady();
}

bool AalVideoRendererControl::isPreviewStarted() const
{
    return m_previewStarted;
}

void AalVideoRendererControl::updateViewfinderFrame()
{
    if (!m_service->viewfinderControl()) {
        qWarning() << "Can't draw video frame without a viewfinder settings control";
        return;
    }
    if (!m_service->androidControl()) {
        qWarning() << "Can't draw video frame without camera";
        return;
    }
    if (!m_surface) {
        qWarning() << "Can't draw video frame without surface";
        return;
    }

    QSize vfSize = m_service->viewfinderControl()->currentSize();
    QVideoFrame frame(new AalGLTextureBuffer(m_textureId,
                                             vfSize.width(), vfSize.height()),
                      vfSize, QVideoFrame::Format_RGB32);

    if (!frame.isValid()) {
        qWarning() << "Invalid frame";
        return;
    }

    CameraControl *cc = m_service->androidControl();
    frame.setMetaData("CamControl", QVariant::fromValue((void*)cc));

    if (!m_surface->isActive()) {
        QVideoSurfaceFormat format(frame.size(), frame.pixelFormat(), frame.handleType());

        if (!m_surface->start(format)) {
            qWarning() << "Failed to start viewfinder with format:" << format;
        }
    }

    if (m_surface->isActive()) {
        m_surface->present(frame);
    }
}

void AalVideoRendererControl::onTextureCreated(GLuint textureID)
{
    m_textureId = textureID;
    CameraControl *cc = m_service->androidControl();
    if (cc) {
        android_camera_set_preview_texture(cc, m_textureId);
        if (m_textureId && m_previewStarted) {
            android_camera_start_preview(cc);
        }
    }
    m_service->updateCaptureReady();
}

void AalVideoRendererControl::onSnapshotTaken(QImage snapshotImage)
{
    m_preview = snapshotImage;
    Q_EMIT previewReady();
}

void AalVideoRendererControl::updateViewfinderFrameCB(void* context)
{
    Q_UNUSED(context);
    AalVideoRendererControl *self = AalCameraService::instance()->videoOutputControl();
    if (self->m_previewStarted) {
        QMetaObject::invokeMethod(self, "updateViewfinderFrame", Qt::QueuedConnection);
    }
}

const QImage &AalVideoRendererControl::preview() const
{
    return m_preview;
}

void AalVideoRendererControl::createPreview()
{
    if (!m_textureId || !m_service->androidControl())
        return;

    QSize vfSize = m_service->viewfinderControl()->currentSize();
    SharedSignal::instance()->setSnapshotSize(vfSize);
    SharedSignal::instance()->takeSnapshot(m_service->androidControl());
}
