/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
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

#include "aalimageencodercontrol.h"
#include "aalcameraservice.h"

#include <hybris/camera/camera_compatibility_layer_capabilities.h>

#include <unistd.h>

#include <QDebug>

AalImageEncoderControl::AalImageEncoderControl(AalCameraService *service, QObject *parent)
    : QImageEncoderControl(parent),
      m_service(service),
      m_currentSize(),
      m_currentThumbnailSize()
{
}

AalImageEncoderControl::~AalImageEncoderControl()
{
}

QString AalImageEncoderControl::imageCodecDescription(const QString &codec) const
{
    Q_UNUSED(codec);
    return QString();
}

QImageEncoderSettings AalImageEncoderControl::imageSettings() const
{
    return QImageEncoderSettings();
}

void AalImageEncoderControl::setImageSettings(const QImageEncoderSettings &settings)
{
    Q_UNUSED(settings);
}

QStringList AalImageEncoderControl::supportedImageCodecs() const
{
    return QStringList();
}

QList<QSize> AalImageEncoderControl::supportedResolutions(const QImageEncoderSettings &settings, bool *continuous) const
{
    Q_UNUSED(continuous);
    Q_UNUSED(settings);

    return m_availableSizes;
}

QList<QSize> AalImageEncoderControl::supportedThumbnailResolutions(const QImageEncoderSettings &settings, bool *continuous) const
{
    Q_UNUSED(continuous);
    Q_UNUSED(settings);

    return m_availableThumbnailSizes;
}

void AalImageEncoderControl::init(CameraControl *control)
{
    Q_ASSERT(control != NULL);

    if (m_availableSizes.isEmpty()) {
        android_camera_enumerate_supported_picture_sizes(control, &AalImageEncoderControl::getPictureSizeCb, this);
        android_camera_enumerate_supported_thumbnail_sizes(control, &AalImageEncoderControl::getThumbnailSizeCb, this);
    }
}

void AalImageEncoderControl::setSize(const QSize &size)
{
    CameraControl *cc = m_service->androidControl();
    if (!cc) {
        m_currentSize = size;
        return;
    }

    if (!m_availableSizes.contains(size)) {
        qWarning() << "Size " << size << "is not supported by the camera";
        qWarning() << "Supported sizes are: " << m_availableSizes;
        return;
    }

    m_currentSize = size;

    android_camera_set_picture_size(cc, size.width(), size.height());
}

/*!
 * \brief AalImageEncoderControl::setThumbnailSize sets the resolution of JPEG thumbnail
 */
void AalImageEncoderControl::setThumbnailSize(const QSize &size)
{
    CameraControl *cc = m_service->androidControl();
    if (!cc) {
        m_currentThumbnailSize = size;
        return;
    }

    if (!m_availableThumbnailSizes.contains(size)) {
        qWarning() << "Thumbnail size " << size << "is not supported by the camera";
        qWarning() << "Supported thumbnail sizes are: " << m_availableThumbnailSizes;
        return;
    }

    m_currentThumbnailSize = size;

    android_camera_set_thumbnail_size(cc, size.width(), size.height());
}

void AalImageEncoderControl::resetAllSettings()
{
    m_availableSizes.clear();
    m_availableThumbnailSizes.clear();
    m_currentSize = QSize();
    m_currentThumbnailSize = QSize();
}

/*!
 * \brief AalImageEncoderControl::enablePhotoMode prepares the camera to take photos
 */
void AalImageEncoderControl::enablePhotoMode()
{
    CameraControl *cc = m_service->androidControl();
    if (!cc || !m_currentSize.isValid()) {
        return;
    }
    android_camera_set_picture_size(cc, m_currentSize.width(), m_currentSize.height());
    android_camera_set_thumbnail_size(cc, m_currentThumbnailSize.width(), m_currentThumbnailSize.height());
}

void AalImageEncoderControl::getPictureSizeCb(void *ctx, int width, int height)
{
    if (ctx != NULL)
    {
        AalImageEncoderControl *self = static_cast<AalImageEncoderControl *>(ctx);
        self->getPictureSize(width, height);
    }
    else
        qWarning() << "ctx is NULL, cannot get supported camera resolutions." << endl;
}

void AalImageEncoderControl::getThumbnailSizeCb(void *ctx, int width, int height)
{
    if (ctx != NULL)
    {
        AalImageEncoderControl *self = static_cast<AalImageEncoderControl *>(ctx);
        self->getThumbnailSize(width, height);
    }
    else
        qWarning() << "ctx is NULL, cannot get supported thumbnail resolutions." << endl;
}

void AalImageEncoderControl::getPictureSize(int width, int height)
{
    m_availableSizes.append(QSize(width, height));
}

void AalImageEncoderControl::getThumbnailSize(int width, int height)
{
    m_availableThumbnailSizes.append(QSize(width, height));
}
