#include "cameraprocessingsv.h"

#include <QDir>
#include <QDebug>
#include <QThread>
#include <opencv2/opencv.hpp>

QString getCurrentTimestampSV() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
}

QVector<int> getConnectedCameras() {
    QVector<int> cameras;
    int cameraIndex = 0;
    while (true) {
        cv::VideoCapture cap(cameraIndex);
        if (cap.isOpened()) {
            cameras.append(cameraIndex);
            cap.release();
        } else {
            break;
        }
        cameraIndex++;
    }
    return cameras;
}

double getCameraFPS(int cameraIndex) {
    cv::VideoCapture cap(cameraIndex);
    if (!cap.isOpened()) {
        qWarning() << "Failed to open camera" << cameraIndex << "for FPS detection";
        return 30.0;
    }
    double fps = cap.get(cv::CAP_PROP_FPS);
    cap.release();
    if (fps <= 0 || fps > 120) {
        qWarning() << "Invalid or unsupported FPS for camera" << cameraIndex << ". Using default FPS: 30";
        return 30.0;
    }
    qDebug() << "Detected FPS for camera" << cameraIndex << ":" << fps;
    return fps;
}

void capturePhotoFromAllCameras(const QVector<int>& cameras) {
    if (cameras.isEmpty()) {
        qWarning() << "No cameras found!";
        return;
    }
    for (int cameraIndex : cameras) {
        cv::VideoCapture cap(cameraIndex);
        if (!cap.isOpened()) {
            qWarning() << "Failed to open camera" << cameraIndex;
            continue;
        }
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) {
            qWarning() << "Failed to capture frame from camera" << cameraIndex;
            cap.release();
            continue;
        }
        QString filename = QString("photo_camera_%1.jpg").arg(cameraIndex);
        if (cv::imwrite(filename.toStdString(), frame)) {
            qDebug() << "Photo captured from camera" << cameraIndex << "and saved as" << filename;
        } else {
            qWarning() << "Failed to save photo for camera" << cameraIndex;
        }
        cap.release();
    }
}

void capturePhotoFromAllCameras(const QVector<int>& cameras, const QString& basePath) {
    if (cameras.isEmpty()) {
        qWarning() << "No cameras found!";
        return;
    }
    QDir dir(basePath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << basePath;
            return;
        }
    }
    for (int cameraIndex : cameras) {
        cv::VideoCapture cap(cameraIndex);
        if (!cap.isOpened()) {
            qWarning() << "Failed to open camera" << cameraIndex;
            continue;
        }
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) {
            qWarning() << "Failed to capture frame from camera" << cameraIndex;
            cap.release();
            continue;
        }
        QString filename = QString("%1/photo_camera_%2.jpg").arg(basePath).arg(cameraIndex);
        if (cv::imwrite(filename.toStdString(), frame)) {
            qDebug() << "Photo captured from camera" << cameraIndex << "and saved as" << filename;
        } else {
            qWarning() << "Failed to save photo for camera" << cameraIndex;
        }
        cap.release();
    }
}

void recordVideoAVI(const QVector<int>& cameras, const QString& basePath, int durationSeconds, int fps) {
    if (cameras.isEmpty()) {
        qWarning() << "No cameras found!";
        return;
    }
    QDir dir(basePath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << basePath;
            return;
        }
    }
    int frameDurationMs = 1000 / fps;
    int totalFrames = durationSeconds * fps;
    for (int cameraIndex : cameras) {
        cv::VideoCapture cap(cameraIndex);
        if (!cap.isOpened()) {
            qWarning() << "Failed to open camera" << cameraIndex;
            continue;
        }
        int frameWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int frameHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        QString filename = QString("%1/video_camera_%2.avi").arg(basePath).arg(cameraIndex);
        cv::VideoWriter writer(
            filename.toStdString(),
            cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
            fps,
            cv::Size(frameWidth, frameHeight)
            );
        if (!writer.isOpened()) {
            qWarning() << "Failed to open video writer for camera" << cameraIndex;
            cap.release();
            continue;
        }
        qDebug() << "Capturing video from camera" << cameraIndex << "to file:" << filename;
        for (int frameIndex = 0; frameIndex < totalFrames; ++frameIndex) {
            auto frameStartTime = std::chrono::high_resolution_clock::now();

            cv::Mat frame;
            cap >> frame;

            if (frame.empty()) {
                qWarning() << "Failed to capture frame from camera" << cameraIndex << "at frame" << frameIndex;
                break;
            }
            writer.write(frame);
            auto frameEndTime = std::chrono::high_resolution_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(frameEndTime - frameStartTime);
            int remainingTime = frameDurationMs - static_cast<int>(elapsedTime.count());
            if (remainingTime > 0) {
                QThread::msleep(remainingTime);
            }
        }
        qDebug() << "Finished capturing video from camera" << cameraIndex;
        cap.release();
        writer.release();
    }
}

void recordVideoMP4(const QVector<int>& cameras, const QString& basePath, int durationSeconds, int fps) {
    if (cameras.isEmpty()) {
        qWarning() << "No cameras found!";
        return;
    }
    QDir dir(basePath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << basePath;
            return;
        }
    }
    int frameDurationMs = 1000 / fps;
    int totalFrames = durationSeconds * fps;
    for (int cameraIndex : cameras) {
        cv::VideoCapture cap(cameraIndex);
        if (!cap.isOpened()) {
            qWarning() << "Failed to open camera" << cameraIndex;
            continue;
        }
        int frameWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int frameHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        QString filename = QString("%1/video_camera_%2.mp4").arg(basePath).arg(cameraIndex);
        cv::VideoWriter writer(
            filename.toStdString(),
            cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
            fps,
            cv::Size(frameWidth, frameHeight)
            );
        if (!writer.isOpened()) {
            qWarning() << "Failed to open video writer for camera" << cameraIndex;
            cap.release();
            continue;
        }
        qDebug() << "Recording video from camera" << cameraIndex << "to file:" << filename;
        for (int frameIndex = 0; frameIndex < totalFrames; ++frameIndex) {
            auto frameStartTime = std::chrono::high_resolution_clock::now();
            cv::Mat frame;
            cap >> frame;
            if (frame.empty()) {
                qWarning() << "Failed to capture frame from camera" << cameraIndex << "at frame" << frameIndex;
                break;
            }
            writer.write(frame);
            auto frameEndTime = std::chrono::high_resolution_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(frameEndTime - frameStartTime);
            int remainingTime = frameDurationMs - static_cast<int>(elapsedTime.count());
            if (remainingTime > 0) {
                QThread::msleep(remainingTime);
            }
        }
        qDebug() << "Finished recording video from camera" << cameraIndex;
        cap.release();
        writer.release();
    }
}

QList<QPair<QString, QByteArray>> capturePhotoFromAllCameras() {
    QList<QPair<QString, QByteArray>> photos;
    QVector<int> cameras = getConnectedCameras();
    if (cameras.isEmpty()) {
        qWarning() << "No cameras found!";
        return photos;
    }
    for (int cameraIndex : cameras) {
        cv::VideoCapture cap(cameraIndex);
        if (!cap.isOpened()) {
            qWarning() << "Failed to open camera" << cameraIndex;
            continue;
        }
        cv::Mat frame;
        cap >> frame;
        if (frame.empty()) {
            qWarning() << "Failed to capture frame from camera" << cameraIndex;
            cap.release();
            continue;
        }
        std::vector<uchar> buf;
        cv::imencode(".jpg", frame, buf);
        QByteArray imageData(reinterpret_cast<const char*>(buf.data()), buf.size());
        QString fileName = QString("photo_camera_%1_%2.jpg").arg(cameraIndex).arg(getCurrentTimestampSV());
        photos.append(qMakePair(fileName, imageData));
        cap.release();
    }
    return photos;
}

QList<QString> recordVideoFromAllCameras(const QString& basePath, int durationSeconds, int fps) {
    QList<QString> videoPaths;
    QVector<int> cameras = getConnectedCameras();
    if (cameras.isEmpty()) {
        qWarning() << "No cameras found!";
        return videoPaths;
    }
    QDir dir(basePath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << basePath;
            return videoPaths;
        }
    }
    int frameDurationMs = 1000 / fps;
    int totalFrames = durationSeconds * fps;
    for (int cameraIndex : cameras) {
        cv::VideoCapture cap(cameraIndex);
        if (!cap.isOpened()) {
            qWarning() << "Failed to open camera" << cameraIndex;
            continue;
        }
        int frameWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        int frameHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        QString filename = QString("video_camera_%1_%2.mp4").arg(cameraIndex).arg(getCurrentTimestampSV());
        cv::VideoWriter writer(
            filename.toStdString(),
            cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
            fps,
            cv::Size(frameWidth, frameHeight)
            );
        if (!writer.isOpened()) {
            qWarning() << "Could not open VideoWriter for camera" << cameraIndex;
            cap.release();
            continue;
        }
        qDebug() << "Record video from camera" << cameraIndex << "to file:" << filename;
        for (int frameIndex = 0; frameIndex < totalFrames; ++frameIndex) {
            auto frameStartTime = std::chrono::high_resolution_clock::now();

            cv::Mat frame;
            cap >> frame;

            if (frame.empty()) {
                qWarning() << "Failed to capture frame from camera" << cameraIndex << "on frame" << frameIndex;
                break;
            }
            writer.write(frame);
            auto frameEndTime = std::chrono::high_resolution_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(frameEndTime - frameStartTime);
            int remainingTime = frameDurationMs - static_cast<int>(elapsedTime.count());
            if (remainingTime > 0) {
                QThread::msleep(remainingTime);
            }
        }
        qDebug() << "Record video from camera" << cameraIndex << "completed.";
        cap.release();
        writer.release();
        videoPaths.append(filename);
    }
    return videoPaths;
}
