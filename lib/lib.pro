TEMPLATE = lib

QT  -= gui

TARGET = qpmx

DEFINES += QPMX_LIBRARY

HEADERS += \
	libqpmx_global.h \
	libqpmx.h

SOURCES +=

CONFIG += qtcoroutines_exported
include(../submodules/qtcoroutines/qtcoroutines.pri)

