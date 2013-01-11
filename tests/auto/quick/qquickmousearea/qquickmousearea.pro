CONFIG += testcase
TARGET = tst_qquickmousearea
macx:CONFIG -= app_bundle

HEADERS += ../../shared/testhttpserver.h
SOURCES += tst_qquickmousearea.cpp \
           ../../shared/testhttpserver.cpp

include (../../shared/util.pri)

TESTDATA = data/*

QT += core-private gui-private qml-private quick-private network testlib
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
