#pragma once

#include <QObject>
#include <QTcpSocket>
#include "server/mediaserver.h"
#include "service/mediaservice.h"

class MediaController : public QObject {
    Q_OBJECT

public:
    explicit MediaController(MediaServer* server, MediaService* service, QObject* parent = nullptr);

private slots:
    void handleCommand(const QString& command, QTcpSocket* clientSocket);

private:
    MediaServer* server;
    MediaService* service;

    void sendTextResponse(QTcpSocket* clientSocket, const QString& response);
    void sendFileResponse(QTcpSocket* clientSocket, const QString& fileName, const QByteArray& fileData);
    void sendFileResponse(QTcpSocket* clientSocket, const QString& fileName, const QString& filePath);
};
