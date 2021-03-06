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

#include <QtTest/QtTest>

#include "aalcameraservice.h"

#define private public
#include "aalviewfindersettingscontrol.h"

class tst_AalViewfinderSettingsControl : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void setSize();
    void resetAllSettings();

    void chooseOptimalSize16by9();
    void chooseOptimalSize4by3();
    void chooseOptimalSizeEmpty();
    void chooseOptimalSize0AspectRatio();
    void chooseOptimalSize0AspectRatioEmpty();

private:
    AalViewfinderSettingsControl *m_vfControl;
    AalCameraService *m_service;

    friend class AalViewfinderSettingsControl;
};

void tst_AalViewfinderSettingsControl::initTestCase()
{
    m_service = new AalCameraService();
    m_vfControl = new AalViewfinderSettingsControl(m_service);
}

void tst_AalViewfinderSettingsControl::cleanupTestCase()
{
    delete m_vfControl;
    delete m_service;
}

void tst_AalViewfinderSettingsControl::setSize()
{
    m_vfControl->m_currentSize = QSize(123, 234);

    QSize size(640, 480);
    m_vfControl->setViewfinderParameter(QCameraViewfinderSettingsControl::Resolution, size);
    QSize result = m_vfControl->viewfinderParameter(QCameraViewfinderSettingsControl::Resolution).toSize();

    QCOMPARE(result, size);
    QCOMPARE(result, m_vfControl->currentSize());
}

void tst_AalViewfinderSettingsControl::resetAllSettings()
{
    m_vfControl->m_currentSize = QSize(123, 234);
    m_vfControl->resetAllSettings();
    QCOMPARE(m_vfControl->currentSize(), QSize());
}

void tst_AalViewfinderSettingsControl::chooseOptimalSize16by9()
{
    m_vfControl->m_aspectRatio = (float)16 / (float)9;
    QList<QSize> resolutions;
    resolutions.append(QSize(1920, 1080));
    resolutions.append(QSize(1280, 720));
    resolutions.append(QSize(960, 720));

    QCOMPARE(m_vfControl->chooseOptimalSize(resolutions), QSize(1920, 1080));
}

void tst_AalViewfinderSettingsControl::chooseOptimalSize4by3()
{
    m_vfControl->m_aspectRatio = (float)4 / (float)3;
    QList<QSize> resolutions;
    resolutions.append(QSize(1920, 1080));
    resolutions.append(QSize(1280, 720));
    resolutions.append(QSize(960, 720));

    QCOMPARE(m_vfControl->chooseOptimalSize(resolutions), QSize(960, 720));
}

void tst_AalViewfinderSettingsControl::chooseOptimalSizeEmpty()
{
    m_vfControl->m_aspectRatio = (float)4 / (float)3;
    QList<QSize> resolutions;

    QCOMPARE(m_vfControl->chooseOptimalSize(resolutions), QSize());
}

void tst_AalViewfinderSettingsControl::chooseOptimalSize0AspectRatio()
{
    m_vfControl->m_aspectRatio = 0;
    QList<QSize> resolutions;
    resolutions.append(QSize(1920, 1080));
    resolutions.append(QSize(1280, 720));
    resolutions.append(QSize(960, 720));

    QCOMPARE(m_vfControl->chooseOptimalSize(resolutions), QSize(1280, 720));
}

void tst_AalViewfinderSettingsControl::chooseOptimalSize0AspectRatioEmpty()
{
    m_vfControl->m_aspectRatio = 0;
    QList<QSize> resolutions;

    QCOMPARE(m_vfControl->chooseOptimalSize(resolutions), QSize());
}

QTEST_GUILESS_MAIN(tst_AalViewfinderSettingsControl)

#include "tst_aalviewfindersettingscontrol.moc"
