TEMPLATE = lib

QT += core
QT -= gui

TARGET = gitsource
VERSION = $$QPMXVER
CONFIG += plugin

DESTDIR = $$OUT_PWD/../qpmx

HEADERS += \
		gitsourceplugin.h

SOURCES += \
		gitsourceplugin.cpp

DISTFILES += gitsource.json

include(../../install.pri)
target.path = $${INSTALL_PLUGINS}/qpmx
INSTALLS += target
