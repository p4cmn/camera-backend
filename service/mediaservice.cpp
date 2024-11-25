
#include "mediaservice.h"

MediaService::MediaService(QObject* parent)
    : QObject(parent) {
}

QString MediaService::getAllCamerasInfo() {
    return ::getAllCamerasInfo();
}

QList<QPair<QString, QByteArray>> MediaService::capturePhotoFromAllCameras() {
    return ::capturePhotoFromAllCameras();
}

QList<QString> MediaService::recordVideoFromAllCameras(const QString& basePath, int durationSeconds, int fps) {
    return ::recordVideoFromAllCameras(basePath, durationSeconds, fps);
}

QList<QString> MediaService::recordVideoWithAudioFromAllCameras(const QString& basePath, int durationSeconds, UINT32 fps) {
    return ::recordVideoWithAudioFromAllCameras(basePath, durationSeconds, fps);
}
