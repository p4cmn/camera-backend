#pragma once

#include <QVector>
#include <QString>

QVector<int> getConnectedCameras();
double getCameraFPS(int cameraIndex);
void capturePhotoFromAllCameras(const QVector<int>& cameras, const QString& basePath);
void recordVideoAVI(const QVector<int>& cameras, const QString& basePath, int durationSeconds = 5, int fps = 30);
void recordVideoMP4(const QVector<int>& cameras, const QString& basePath, int durationSeconds = 5, int fps = 30);
QList<QPair<QString, QByteArray>> capturePhotoFromAllCameras();
QList<QString> recordVideoFromAllCameras(const QString& basePath, int durationSeconds, int fps);
