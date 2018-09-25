TEMPLATE = lib

QT  -= gui
CONFIG += skip_target_version_ext
mac: CONFIG += static

TARGET = qpmx
QMAKE_TARGET_DESCRIPTION = "qpmx connection library"

DEFINES += QPMX_LIBRARY
mac: DEFINES += _XOPEN_SOURCE

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

include(../submodules/deployment/install.pri)
target.path = $$INSTALL_LIBS
dlltarget.path = $$INSTALL_BINS
tHeaders.path = $${INSTALL_HEADERS}/qpmx
tHeaders.files = $$QPMX_PUBLIC_HEADERS $$PUBLIC_HEADERS
INSTALLS += target dlltarget tHeaders
