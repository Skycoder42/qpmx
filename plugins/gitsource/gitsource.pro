TEMPLATE = lib

QT += core
QT -= gui

TARGET = gitsource
CONFIG += plugin

DESTDIR = $$OUT_PWD/../qpmx

HEADERS += \
	gitsourceplugin.h

SOURCES += \
	gitsourceplugin.cpp

include(../../lib.pri)

DISTFILES += gitsource.json
json_target.target = moc_gitsourceplugin.o
json_target.depends += $$PWD/gitsource.json
QMAKE_EXTRA_TARGETS += json_target

include(../../install.pri)
target.path = $${INSTALL_PLUGINS}/qpmx
INSTALLS += target
