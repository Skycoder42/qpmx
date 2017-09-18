# Defined in .qmake.conf: QPMX_*
TEMPLATE = lib
CONFIG += staticlib
win32: CONFIG += debug_and_release

TARGET = $$qtLibraryTarget($$QPMX_TARGET)
VERSION = $$QPMX_VERSION

CONFIG += qpmx_static
include($$QPMX_PRI_INCLUDE)

# add qtbase modules
QT *= core gui widgets network xml sql concurrent

# install stuff
isEmpty(PUBLIC_HEADERS): PUBLIC_HEADERS = $$HEADERS

target.path = $$QPMX_INSTALL/lib
headers.files = $$PUBLIC_HEADERS
headers.path = $$QPMX_INSTALL/include

INSTALLS += headers
isEmpty(SOURCES):isEmpty(GENERATED_SOURCES):write_file($$OUT_PWD/.no_sources_detected)
else: INSTALLS += target

installall.target = all-install
win32 {
	CONFIG += debug_and_release
	installall.depends += release-install debug-install
} else: installall.depends += install
QMAKE_EXTRA_TARGETS += installall

#translations
TRANSLATIONS += $$QPMX_TRANSLATIONS

isEmpty(LRELEASE) {
	qtPrepareTool(LRELEASE, lrelease)
	LRELEASE += -nounfinished
}
ts_target.target = lrelease
ts_target.commands = $$LRELEASE $$_PRO_FILE_
QMAKE_EXTRA_TARGETS += ts_target

ts_install.path = $$QPMX_INSTALL/translations
ts_install.depends += lrelease
ts_install.CONFIG += no_check_exist
for(tsfile, TRANSLATIONS): ts_install.files += "$$replace(tsfile, .ts, .qm)"
INSTALLS += ts_install
