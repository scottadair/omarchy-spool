QT += core gui qml quick quickcontrols2 dbus concurrent

CONFIG += c++17 release
TARGET = spool
TEMPLATE = app
LIBS += -lcups

HEADERS += \
    src/admin.h \
    src/backend.h \
    src/cupsdata.h \
    src/printeraddress.h \
    src/rowmodel.h \
    src/theme.h

SOURCES += \
    src/admin.cpp \
    src/backend.cpp \
    src/cupsdata.cpp \
    src/main.cpp \
    src/printeraddress.cpp \
    src/rowmodel.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc
