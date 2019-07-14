/*
 * Copyright (C) 2013 Canonical, Ltd.
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

#include "aalcameraserviceplugin.h"
#include "aalcameraservice.h"

#include <QString>
#include <QHash>
#include <QDebug>
#include <QMetaType>
#include <qgl.h>

#include <hybris/properties/properties.h>
#include <hybris/camera/camera_compatibility_layer.h>
#include <hybris/camera/camera_compatibility_layer_capabilities.h>

AalServicePlugin::AalServicePlugin()
{
}

QMediaService* AalServicePlugin::create(QString const& key)
{
    if (key == QLatin1String(Q_MEDIASERVICE_CAMERA))
        return new AalCameraService;
    else
        qWarning() << "Key not supported:" << key;

    return 0;
}

void AalServicePlugin::release(QMediaService *service)
{
    delete service;
}

QList<QByteArray> AalServicePlugin::devices(const QByteArray &service) const
{
    QList<QByteArray> deviceList;

    if (QString::fromLatin1(service) != QLatin1String(Q_MEDIASERVICE_CAMERA)) {
        return deviceList;
    }

    // Devices are identified in android only by their index, so we do the same
    int cameras = android_camera_get_number_of_devices();
    for (int deviceId = 0; deviceId < cameras; deviceId++) {
        QString camera("%1");
        camera = camera.arg(deviceId);
        deviceList.append(camera.toLatin1());
    }

    return deviceList;
}

QString AalServicePlugin::deviceDescription(const QByteArray &service, const QByteArray &device)
{
    if (QString::fromLatin1(service) != QLatin1String(Q_MEDIASERVICE_CAMERA)) {
        return QString();
    }

    // Android does not provice a descriptive identifier for devices, so we just
    // send back the index plus some useful human readable information about position.
    bool ok;
    int deviceID = device.toInt(&ok, 10);
    if (!ok || deviceID >= android_camera_get_number_of_devices()) {
        qWarning() << "Requested description for invalid device ID:" << device;
        return QString();
    } else {
        QCamera::Position position = cameraPosition(device);
        return QString("Camera %1%2").arg(QLatin1String(device))
                                     .arg(position == QCamera::FrontFace ? " Front facing" :
                                          (position == QCamera::BackFace ? " Back facing" : ""));
    }
}

// krillin / vegetahd lies to us - the top of the front facing camera
// points to the right of the screen (viewed from the front), which means
// the camera image needs rotating by 270deg with the device in its natural
// orientation (portrait). It tells us the camera orientation is 90deg
// though (see https://launchpad.net/bugs/1567542)
// https://git.launchpad.net/oxide/tree/shared/browser/media/oxide_video_capture_device_hybris.cc#n92

// Meanwhile, cooler (M10 HD) gives the orientation of 0 for all cameras while
// actually has its cameras pointing toward the "bottom" of the device.
// Except, both the screen and the orientation sensor agrees that the native
// orientation is "portrait", not the apparent "landscape". This means the
// back camera's returned orientation must be 270 deg (Qt convention), while
// the front one's must be 90.

// This map contains all overrides we have. The format for the key is
// "<device codename>_<camera_id>" where the camera id is usually "0" for back
// facing camera and "1" for front facing one. The value contains the orientation
// we would return (in Qt convention i.e. no conversion required).
static const QHash<QString, int> cameraOrientationOverride = {
    {"krillin_1", 90},
    {"vegetahd_1", 90},
    {"cooler_0", 270},
    {"cooler_1", 90},
};

static QString getCameraOrientationOverrideKey(const QString & cameraId) {
    char deviceCodename[PROP_VALUE_MAX];

    int length = property_get("ro.product.device", deviceCodename, "");
    if (length <= 0) {
        return QString();
    }

    return QString("%1_%2").arg(deviceCodename).arg(cameraId);
}

int AalServicePlugin::cameraOrientation(const QByteArray & device) const
{
    QString cameraOrientationOverrideKey = getCameraOrientationOverrideKey(device);
    if (cameraOrientationOverride.contains(cameraOrientationOverrideKey)) {
        return cameraOrientationOverride.value(cameraOrientationOverrideKey);
    }

    bool ok;
    int deviceID = device.toInt(&ok, 10);
    if (!ok) {
        return 0;
    }

    int facing;
    int orientation;

    int result = android_camera_get_device_info(deviceID, &facing, &orientation);
    if (result != 0) {
        return 0;
    }

    // Android's orientation means differently compared to QT's orientation.
    // On Android, it means "the angle that the camera image needs to be
    // rotated", but on QT, it means "the physical orientation of the camera
    // sensor". So, the value will have to be inverted.

    return (360 - orientation) % 360;
}

QCamera::Position AalServicePlugin::cameraPosition(const QByteArray & device) const
{
    int facing;
    int orientation;

    bool ok;
    int deviceID = device.toInt(&ok, 10);
    if (!ok) {
        return QCamera::UnspecifiedPosition;
    }

    int result = android_camera_get_device_info(deviceID, &facing, &orientation);
    if (result != 0) {
        return QCamera::UnspecifiedPosition;
    } else {
        return facing == BACK_FACING_CAMERA_TYPE ? QCamera::BackFace :
                                                   QCamera::FrontFace;
    }
}
