TEMPLATE = lib

QT  -= gui

TARGET = qpmx

DEFINES += QPMX_LIBRARY

QPMX_PUBLIC_HEADERS = \
	libqpmx_global.h \
	libqpmx.h \
	packageinfo.h \
	sourceplugin.h

HEADERS += $$QPMX_PUBLIC_HEADERS \
    qpmxbridge_p.h

SOURCES += \
	packageinfo.cpp \
	sourceplugin.cpp \
    libqpmx.cpp \
    qpmxbridge.cpp

CONFIG += qtcoroutines_exported
include(../submodules/qtcoroutines/qtcoroutines.pri)

target.path = $${INSTALL_LIBS}
tHeaders.path = $${INSTALL_HEADERS}/qpmx
tHeaders.files = $$QPMX_PUBLIC_HEADERS $$PUBLIC_HEADERS
INSTALLS += target tHeaders
