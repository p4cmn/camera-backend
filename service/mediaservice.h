#pragma once


#include <QList>
#include <QPair>
#include <QString>
#include <QObject>
#include <QByteArray>

#include "cameraprocessing.h"
#include "cameraprocessingsv.h"

class MediaService : public QObject {
    Q_OBJECT

public:
    explicit MediaService(QObject* parent = nullptr);

    QString getAllCamerasInfo();

    QList<QPair<QString, QByteArray>> capturePhotoFromAllCameras();

    QList<QString> recordVideoFromAllCameras(const QString& basePath, int durationSeconds, int fps);

    QList<QString> recordVideoWithAudioFromAllCameras(const QString& basePath, int durationSeconds, UINT32 fps);
};
