/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef JCAMERA_H
#define JCAMERA_H

#include <qobject.h>
#include <QtPlatformSupport/private/qjniobject_p.h>
#include <qsize.h>

QT_BEGIN_NAMESPACE

class JCamera : public QObject, public QJNIObject
{
    Q_OBJECT
public:
    enum CameraFacing {
        CameraFacingBack = 0,
        CameraFacingFront = 1
    };

    ~JCamera();

    static JCamera *open(int cameraId);

    void lock();
    void unlock();
    void reconnect();
    void release();

    CameraFacing getFacing();
    int getNativeOrientation();

    QSize getPreferredPreviewSizeForVideo();
    QList<QSize> getSupportedPreviewSizes();

    QSize previewSize() const { return m_previewSize; }
    void setPreviewSize(const QSize &size);
    void setPreviewTexture(jobject surfaceTexture);

    bool isZoomSupported();
    int getMaxZoom();
    QList<int> getZoomRatios();
    int getZoom();
    void setZoom(int value);

    QStringList getSupportedFlashModes();
    QString getFlashMode();
    void setFlashMode(const QString &value);

    int getExposureCompensation();
    void setExposureCompensation(int value);
    float getExposureCompensationStep();
    int getMinExposureCompensation();
    int getMaxExposureCompensation();

    QStringList getSupportedSceneModes();
    QString getSceneMode();
    void setSceneMode(const QString &value);

    QStringList getSupportedWhiteBalance();
    QString getWhiteBalance();
    void setWhiteBalance(const QString &value);

    void setRotation(int rotation);

    QList<QSize> getSupportedPictureSizes();
    void setPictureSize(const QSize &size);
    void setJpegQuality(int quality);

    void startPreview();
    void stopPreview();

    void takePicture();

    static bool initJNI(JNIEnv *env);

Q_SIGNALS:
    void pictureExposed();
    void pictureCaptured(const QByteArray &data);

private:
    JCamera(int cameraId, jobject cam);
    void applyParameters();

    QStringList callStringListMethod(const char *methodName);

    int m_cameraId;
    QJNIObject *m_info;
    QJNIObject *m_parameters;

    QSize m_previewSize;
};

QT_END_NAMESPACE

#endif // JCAMERA_H
