QT += core gui quick quickcontrols2 testlib dbus concurrent
CONFIG += c++17 testcase
TARGET = spool_tests
TEMPLATE = app
LIBS += -lcups

INCLUDEPATH += ../src

HEADERS += ../src/admin.h ../src/backend.h ../src/cupsdata.h ../src/printeraddress.h ../src/rowmodel.h ../src/theme.h
SOURCES += spool_tests.cpp ../src/admin.cpp ../src/backend.cpp ../src/cupsdata.cpp ../src/printeraddress.cpp ../src/rowmodel.cpp ../src/theme.cpp
