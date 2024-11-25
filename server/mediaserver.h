#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class MediaServer : public QObject {
    Q_OBJECT

private:
    QTcpServer* tcpServer;
    QList<QTcpSocket*> clients;

public:
    explicit MediaServer(QObject* parent = nullptr);
    ~MediaServer();

    void start(const QString& address, quint16 port);
    void stop();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

signals:
    void commandReceived(const QString& command, QTcpSocket* clientSocket);
};
