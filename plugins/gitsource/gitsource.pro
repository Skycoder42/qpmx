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

target.path = $$[QT_INSTALL_PLUGINS]/qpmx
INSTALLS += target
