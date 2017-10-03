# Defined in .qmake.conf: QPMX_*
TEMPLATE = lib
CONFIG += staticlib
win32: CONFIG += debug_and_release

# add qtbase modules
QT *= core gui widgets network xml sql concurrent

TARGET = $$qtLibraryTarget($$QPMX_TARGET)
VERSION = $$QPMX_VERSION

CONFIG += qpmx_static
include($$QPMX_PRI_INCLUDE)

# startup hook
qpmx_startup_hook {
	DEFINES += "\'QPMX_STARTUP_HOOK(x)=namespace __qpmx_startup_hooks{void hook_$${QPMX_PKG_HASH}(){x();}}\'"
	write_file($$OUT_PWD/.qpmx_startup_defined)
} else: DEFINES += "\'QPMX_STARTUP_HOOK(x)=static_assert(false, \"add CONFIG += qpmx_startup_hook to your pri-file to enable qpmx startup hooks\");\'"

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
isEmpty(LRELEASE) {
	qtPrepareTool(LRELEASE, lrelease)
	LRELEASE += -nounfinished
}

lrelease_compiler.name = lrelease ${QMAKE_FILE_IN}
lrelease_compiler.input = TRANSLATIONS
lrelease_compiler.variable_out = TRANSLATIONS_QM
lrelease_compiler.commands = $$LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
lrelease_compiler.output = $$OUT_PWD/${QMAKE_FILE_BASE}.qm
lrelease_compiler.CONFIG += no_link target_predeps

QMAKE_EXTRA_COMPILERS += lrelease_compiler

ts_install.path = $$QPMX_INSTALL/translations
ts_install.CONFIG += no_check_exist
#ts_install.files = $$TRANSLATIONS_QM
for(tsfile, TRANSLATIONS) {
	tsBase = $$basename(tsfile)
	ts_install.files += "$$OUT_PWD/$$replace(tsBase, .ts, .qm)"
}
INSTALLS += ts_install
