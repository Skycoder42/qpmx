TEMPLATE = lib

QT += core
QT -= gui

TARGET = qpmsource
VERSION = $$QPMXVER
CONFIG += plugin

DESTDIR = $$OUT_PWD/../qpmx

HEADERS += \
		qpmsourceplugin.h

SOURCES += \
		qpmsourceplugin.cpp

DISTFILES += qpmsource.json

include(../../install.pri)
target.path = $${INSTALL_PLUGINS}/qpmx
INSTALLS += target
