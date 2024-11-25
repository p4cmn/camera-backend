
#include <QDebug>
#include <QHostAddress>

#include "mediaserver.h"

MediaServer::MediaServer(QObject* parent)
    : QObject(parent), tcpServer(new QTcpServer(this)) {
    connect(tcpServer, &QTcpServer::newConnection, this, &MediaServer::onNewConnection);
}

MediaServer::~MediaServer() {
    stop();
}

void MediaServer::start(const QString& address, quint16 port) {
    QHostAddress hostAddress(address);
    if (tcpServer->listen(hostAddress, port)) {
        qDebug() << "The server is running on the port" << port;
    } else {
        qCritical() << "Failed to start server:" << tcpServer->errorString();
    }
}

void MediaServer::stop() {
    if (tcpServer->isListening()) {
        tcpServer->close();
    }
    for (QTcpSocket* client : std::as_const(clients)) {
        client->disconnectFromHost();
        client->deleteLater();
    }
    clients.clear();
    qDebug() << "The server has stopped.";
}

void MediaServer::onNewConnection() {
    QTcpSocket* clientSocket = tcpServer->nextPendingConnection();
    clients.append(clientSocket);
    connect(clientSocket, &QTcpSocket::readyRead, this, &MediaServer::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, this, &MediaServer::onClientDisconnected);
    qDebug() << "The new client has connected.";
}

void MediaServer::onReadyRead() {
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) {
        qWarning() << "Invalid client socket.";
        return;
    }
    QString command = QString::fromUtf8(clientSocket->readAll()).trimmed();
    emit commandReceived(command, clientSocket);
}

void MediaServer::onClientDisconnected() {
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (clientSocket) {
        clients.removeAll(clientSocket);
        clientSocket->deleteLater();
        qDebug() << "The client has disconnected.";
    }
}
