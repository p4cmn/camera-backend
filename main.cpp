
#include <QCoreApplication>

#include "server/mediaserver.h"
#include "controller/mediacontroller.h"
#include "service/mediaservice.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QString usbIdsFilePath = "D:\\PROGRAMMING\\C++\\QT\\Camera-backend\\usb.ids";
    if (!loadUsbIds(usbIdsFilePath)) {
        qCritical() << "Could not load USB IDs.";
        app.exit(EXIT_FAILURE);
    }

    MediaServer server;
    MediaService service;
    MediaController controller(&server, &service);

    QString address = "127.0.0.1";
    quint16 port = 12345;
    server.start(address, port);

    qDebug() << "Server is running. Waiting for client connections...";

    return app.exec();
}
