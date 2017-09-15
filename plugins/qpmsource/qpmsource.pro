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

no_installer {
	target.path = $$[QT_INSTALL_PLUGINS]/qpmx
	INSTALLS += target
}
