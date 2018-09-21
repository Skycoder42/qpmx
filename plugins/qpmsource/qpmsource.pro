TEMPLATE = lib

QT += core
QT -= gui

TARGET = qpmsource
CONFIG += plugin

DESTDIR = $$OUT_PWD/../qpmx

HEADERS += \
		qpmsourceplugin.h

SOURCES += \
		qpmsourceplugin.cpp

include(../../lib.pri)

DISTFILES += qpmsource.json
json_target.target = moc_qpmsourceplugin.o
json_target.depends += $$PWD/qpmsource.json
QMAKE_EXTRA_TARGETS += json_target

include(../../install.pri)
target.path = $${INSTALL_PLUGINS}/qpmx
INSTALLS += target

