TEMPLATE = lib

QT += core
QT -= gui
CONFIG += plugin

TARGET = qpmsource
QMAKE_TARGET_DESCRIPTION = "qpmx qpm Plug-In"

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

include(../../submodules/deployment/install.pri)
target.path = $${INSTALL_PLUGINS}/qpmx
INSTALLS += target

