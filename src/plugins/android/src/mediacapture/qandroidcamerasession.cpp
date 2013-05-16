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

#include "qandroidcamerasession.h"

#include "jcamera.h"
#include "jactivitystatelistener.h"
#include "jmultimediautils.h"
#include "qandroidvideooutput.h"
#include "qandroidmultimediautils.h"
#include <QtConcurrent/qtconcurrentrun.h>
#include <qfile.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

QAndroidCameraSession::QAndroidCameraSession(QObject *parent)
    : QObject(parent)
    , m_selectedCamera(0)
    , m_camera(0)
    , m_nativeOrientation(0)
    , m_videoOutput(0)
    , m_captureMode(QCamera::CaptureViewfinder)
    , m_state(QCamera::UnloadedState)
    , m_savedState(-1)
    , m_status(QCamera::UnloadedStatus)
    , m_previewStarted(false)
    , m_captureDestination(QCameraImageCapture::CaptureToFile)
    , m_captureImageDriveMode(QCameraImageCapture::SingleImageCapture)
    , m_lastImageCaptureId(0)
    , m_readyForCapture(false)
    , m_captureCanceled(false)
    , m_currentImageCaptureId(-1)
{
    m_activityListener = new JActivityStateListener;
    connect(m_activityListener, SIGNAL(paused()), this, SLOT(onActivityPaused()));
    connect(m_activityListener, SIGNAL(resumed()), this, SLOT(onActivityResumed()));
}

QAndroidCameraSession::~QAndroidCameraSession()
{
    close();
    delete m_activityListener;
}

void QAndroidCameraSession::setCaptureMode(QCamera::CaptureModes mode)
{
    if (m_captureMode == mode || !isCaptureModeSupported(mode))
        return;

    m_captureMode = mode;

    emit captureModeChanged(mode);
}

bool QAndroidCameraSession::isCaptureModeSupported(QCamera::CaptureModes mode) const
{
    return mode == QCamera::CaptureViewfinder
            || mode == (QCamera::CaptureViewfinder & QCamera::CaptureStillImage)
            || mode == (QCamera::CaptureViewfinder & QCamera::CaptureVideo);
}

void QAndroidCameraSession::setState(QCamera::State state)
{
    if (m_state == state)
        return;

    switch (state) {
    case QCamera::UnloadedState:
        close();
        break;
    case QCamera::LoadedState:
    case QCamera::ActiveState:
        if (!m_camera && !open()) {
            emit error(QCamera::CameraError, QStringLiteral("Failed to open camera"));
            return;
        }
        if (state == QCamera::ActiveState)
            startPreview();
        else if (state == QCamera::LoadedState)
            stopPreview();
        break;
    }

     m_state = state;
     emit stateChanged(m_state);
}

bool QAndroidCameraSession::open()
{
    close();

    m_status = QCamera::LoadingStatus;
    emit statusChanged(m_status);

    m_camera = JCamera::open(m_selectedCamera);

    if (m_camera) {
        connect(m_camera, SIGNAL(pictureExposed()), this, SLOT(onCameraPictureExposed()));
        connect(m_camera, SIGNAL(pictureCaptured(QByteArray)), this, SLOT(onCameraPictureCaptured(QByteArray)));
        m_nativeOrientation = m_camera->getNativeOrientation();
        m_status = QCamera::LoadedStatus;
        emit opened();
    } else {
        m_status = QCamera::UnavailableStatus;
    }

    emit statusChanged(m_status);

    return m_camera != 0;
}

void QAndroidCameraSession::close()
{
    if (!m_camera)
        return;

    stopPreview();

    m_status = QCamera::UnloadingStatus;
    emit statusChanged(m_status);

    m_readyForCapture = false;
    m_currentImageCaptureId = -1;
    m_currentImageCaptureFileName.clear();

    m_camera->release();
    delete m_camera;
    m_camera = 0;

    m_status = QCamera::UnloadedStatus;
    emit statusChanged(m_status);
}

void QAndroidCameraSession::setVideoPreview(QAndroidVideoOutput *videoOutput)
{
    if (m_videoOutput)
        m_videoOutput->stop();

    m_videoOutput = videoOutput;
}

void QAndroidCameraSession::startPreview()
{
    if (!m_camera || m_previewStarted)
        return;

    m_status = QCamera::StartingStatus;
    emit statusChanged(m_status);

    applyImageSettings();

    if (m_videoOutput)
        m_camera->setPreviewTexture(m_videoOutput->surfaceTexture());

    JMultimediaUtils::enableOrientationListener(true);

    m_camera->startPreview();
    m_previewStarted = true;

    m_status = QCamera::ActiveStatus;
    emit statusChanged(m_status);

    setReadyForCapture(true);
}

void QAndroidCameraSession::stopPreview()
{
    if (!m_camera || !m_previewStarted)
        return;

    m_status = QCamera::StoppingStatus;
    emit statusChanged(m_status);

    JMultimediaUtils::enableOrientationListener(false);

    m_camera->stopPreview();
    if (m_videoOutput)
        m_videoOutput->stop();
    m_previewStarted = false;

    m_status = QCamera::LoadedStatus;
    emit statusChanged(m_status);

    setReadyForCapture(false);
}

void QAndroidCameraSession::setImageSettings(const QImageEncoderSettings &settings)
{
    if (m_imageSettings == settings)
        return;

    m_imageSettings = settings;
    if (m_imageSettings.codec().isEmpty())
        m_imageSettings.setCodec(QLatin1String("jpeg"));

    applyImageSettings();
}

int QAndroidCameraSession::currentCameraRotation() const
{
    if (!m_camera)
        return 0;

    // subtract natural camera orientation and physical device orientation
    int rotation = 0;
    int deviceOrientation = (JMultimediaUtils::getDeviceOrientation() + 45) / 90 * 90;
    if (m_camera->getFacing() == JCamera::CameraFacingFront)
        rotation = (m_nativeOrientation - deviceOrientation + 360) % 360;
    else // back-facing camera
        rotation = (m_nativeOrientation + deviceOrientation) % 360;

    return rotation;
}

void QAndroidCameraSession::applyImageSettings()
{
    if (!m_camera)
        return;

    const QSize requestedResolution = m_imageSettings.resolution();
    const QList<QSize> supportedResolutions = m_camera->getSupportedPictureSizes();

    if (!requestedResolution.isValid()) {
        // if no resolution is set, use the highest supported one
        m_imageSettings.setResolution(supportedResolutions.last());
    } else if (!supportedResolutions.contains(requestedResolution)) {
        // if the requested resolution is not supported, find the closest one
        int reqPixelCount = requestedResolution.width() * requestedResolution.height();
        QList<int> supportedPixelCounts;
        for (int i = 0; i < supportedResolutions.size(); ++i) {
            const QSize &s = supportedResolutions.at(i);
            supportedPixelCounts.append(s.width() * s.height());
        }
        int closestIndex = qt_findClosestValue(supportedPixelCounts, reqPixelCount);
        m_imageSettings.setResolution(supportedResolutions.at(closestIndex));
    }

    const QSize photoResolution = m_imageSettings.resolution();
    const qreal aspectRatio = static_cast<qreal>(photoResolution.width())/static_cast<qreal>(photoResolution.height());

    // apply viewfinder configuration
    QSize viewfinderResolution;
    QList<QSize> previewSizes = m_camera->getSupportedPreviewSizes();
    for (int i = previewSizes.count() - 1; i >= 0; --i) {
        const QSize &size = previewSizes.at(i);
        // search for viewfinder resolution with the same aspect ratio
        if (qFuzzyCompare(aspectRatio, (static_cast<qreal>(size.width())/static_cast<qreal>(size.height())))) {
            viewfinderResolution = size;
            break;
        }
    }

    if (m_camera->previewSize() != viewfinderResolution) {
        if (m_videoOutput)
            m_videoOutput->setVideoSize(viewfinderResolution);

        // if preview is started, we have to stop it first before changing its size
        if (m_previewStarted)
            m_camera->stopPreview();

        m_camera->setPreviewSize(viewfinderResolution);

        // restart preview
        if (m_previewStarted)
            m_camera->startPreview();
    }

    int jpegQuality = 100;
    switch (m_imageSettings.quality()) {
    case QMultimedia::VeryLowQuality:
        jpegQuality = 20;
        break;
    case QMultimedia::LowQuality:
        jpegQuality = 40;
        break;
    case QMultimedia::NormalQuality:
        jpegQuality = 60;
        break;
    case QMultimedia::HighQuality:
        jpegQuality = 80;
        break;
    case QMultimedia::VeryHighQuality:
        jpegQuality = 100;
        break;
    }

    m_camera->setPictureSize(photoResolution);
    m_camera->setJpegQuality(jpegQuality);
}

bool QAndroidCameraSession::isCaptureDestinationSupported(QCameraImageCapture::CaptureDestinations destination) const
{
    return destination & (QCameraImageCapture::CaptureToFile | QCameraImageCapture::CaptureToBuffer);
}

QCameraImageCapture::CaptureDestinations QAndroidCameraSession::captureDestination() const
{
    return m_captureDestination;
}

void QAndroidCameraSession::setCaptureDestination(QCameraImageCapture::CaptureDestinations destination)
{
    if (m_captureDestination != destination) {
        m_captureDestination = destination;
        emit captureDestinationChanged(m_captureDestination);
    }
}

bool QAndroidCameraSession::isReadyForCapture() const
{
    return m_status == QCamera::ActiveStatus && m_readyForCapture;
}

void QAndroidCameraSession::setReadyForCapture(bool ready)
{
    if (m_readyForCapture == ready)
        return;

    m_readyForCapture = ready;
    emit readyForCaptureChanged(ready);
}

QCameraImageCapture::DriveMode QAndroidCameraSession::driveMode() const
{
    return m_captureImageDriveMode;
}

void QAndroidCameraSession::setDriveMode(QCameraImageCapture::DriveMode mode)
{
    m_captureImageDriveMode = mode;
}

int QAndroidCameraSession::capture(const QString &fileName)
{
    ++m_lastImageCaptureId;

    if (!isReadyForCapture()) {
        emit imageCaptureError(m_lastImageCaptureId, QCameraImageCapture::NotReadyError,
                               tr("Camera not ready"));
        return m_lastImageCaptureId;
    }

    if (m_captureImageDriveMode == QCameraImageCapture::SingleImageCapture) {
        setReadyForCapture(false);

        m_currentImageCaptureId = m_lastImageCaptureId;
        m_currentImageCaptureFileName = fileName;

        // adjust picture rotation depending on the device orientation
        m_camera->setRotation(currentCameraRotation());

        m_camera->takePicture();
    } else {
        emit imageCaptureError(m_lastImageCaptureId, QCameraImageCapture::NotSupportedFeatureError,
                               tr("Drive mode not supported"));
    }

    return m_lastImageCaptureId;
}

void QAndroidCameraSession::cancelCapture()
{
    if (m_readyForCapture)
        return;

    m_captureCanceled = true;
}

void QAndroidCameraSession::onCameraPictureExposed()
{
    if (m_captureCanceled)
        return;

    emit imageExposed(m_currentImageCaptureId);
}

void QAndroidCameraSession::onCameraPictureCaptured(const QByteArray &data)
{
    if (!m_captureCanceled) {
        // generate a preview from the viewport
        if (m_videoOutput)
            emit imageCaptured(m_currentImageCaptureId, m_videoOutput->toImage());

        // Loading and saving the captured image can be slow, do it in a separate thread
        QtConcurrent::run(this, &QAndroidCameraSession::processCapturedImage,
                          m_currentImageCaptureId,
                          data,
                          m_captureDestination,
                          m_currentImageCaptureFileName);
    }

    m_captureCanceled = false;

    // Preview needs to be restarted after taking a picture
    m_camera->startPreview();

    setReadyForCapture(true);
}

void QAndroidCameraSession::processCapturedImage(int id,
                                                 const QByteArray &data,
                                                 QCameraImageCapture::CaptureDestinations dest,
                                                 const QString &fileName)
{


    if (dest & QCameraImageCapture::CaptureToFile) {
        const QString actualFileName = m_mediaStorageLocation.generateFileName(fileName,
                                                                               QCamera::CaptureStillImage,
                                                                               QLatin1String("IMG_"),
                                                                               QLatin1String("jpg"));

        QFile file(actualFileName);
        if (file.open(QFile::WriteOnly)) {
            if (file.write(data) == data.size()) {
                // if the picture is saved into the standard picture location, register it
                // with the Android media scanner so it appears immediately in apps
                // such as the gallery.
                QString standardLoc = JMultimediaUtils::getDefaultMediaDirectory(JMultimediaUtils::DCIM);
                if (actualFileName.startsWith(standardLoc))
                    JMultimediaUtils::registerMediaFile(actualFileName);

                emit imageSaved(id, actualFileName);
            } else {
                emit imageCaptureError(id, QCameraImageCapture::OutOfSpaceError, file.errorString());
            }
        } else {
            const QString errorMessage = tr("Could not open destination file: %1").arg(actualFileName);
            emit imageCaptureError(id, QCameraImageCapture::ResourceError, errorMessage);
        }
    }

    if (dest & QCameraImageCapture::CaptureToBuffer) {
        QImage image;
        const bool ok = image.loadFromData(data, "JPG");

        if (ok) {
            QVideoFrame frame(image);
            emit imageAvailable(id, frame);
        } else {
            emit imageCaptureError(id, QCameraImageCapture::FormatError,
                                   tr("Could not load JPEG data from captured image"));
        }
    }
}

void QAndroidCameraSession::onActivityPaused()
{
    m_savedState = m_state;
    setState(QCamera::UnloadedState);
}

void QAndroidCameraSession::onActivityResumed()
{
    if (m_savedState != -1) {
        setState(QCamera::State(m_savedState));
        m_savedState = -1;
    }
}

QT_END_NAMESPACE