# Defined in .qmake.conf: QPMX_*
TEMPLATE = lib
CONFIG += staticlib
win32: CONFIG += debug_and_release

TARGET = $$qtLibraryTarget($$QPMX_TARGET)
VERSION = $$QPMX_VERSION

CONFIG += qpmx_static
include($$QPMX_PRI_INCLUDE)

isEmpty(PUBLIC_HEADERS): PUBLIC_HEADERS = $$HEADERS

target.path = $$QPMX_INSTALL_LIB
headers.files = $$PUBLIC_HEADERS
headers.path = $$QPMX_INSTALL_INC
INSTALLS += target headers
