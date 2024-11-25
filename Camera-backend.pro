QT = core gui network

CONFIG += c++21 cmdline

INCLUDEPATH += D:/opencv_install/include

LIBS += -LD:/opencv_install/x64/mingw/bin

LIBS += -lopencv_core4100 -lopencv_imgcodecs4100 -lopencv_highgui4100 -lopencv_videoio4100

LIBS += -luuid -lstrmiids -lMfplat -lMf -lMfreadwrite -lDwrite -lole32 -lmfuuid

SOURCES += \
    controller/mediacontroller.cpp \
    main.cpp \
    server/mediaserver.cpp \
    service/cameraprocessing.cpp \
    service/cameraprocessingsv.cpp \
    service/mediaservice.cpp

HEADERS += \
    controller/mediacontroller.h \
    server/mediaserver.h \
    service/cameraprocessing.h \
    service/cameraprocessingsv.h \
    service/mediaservice.h

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    .gitignore
