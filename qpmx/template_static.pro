# Defined in .qmake.conf: QPMX_*
TEMPLATE = lib
CONFIG += staticlib
win32: CONFIG += debug_and_release
else: CONFIG += release

# add modules (from qtbase, qtdeclerative, but only if available)
QT = 
QT_WANTS *= core gui widgets network xml sql concurrent dbus qml quick quickwidgets particles
for(mod, QT_WANTS):qtHaveModule($$mod): QT *= $$mod

TARGET = $$qtLibraryTarget($$QPMX_TARGET)
VERSION = $$QPMX_VERSION

CONFIG += qpmx_static
include($$QPMX_PRI_INCLUDE)

# startup hook (i know, it's ugly...)
REAL_SOURCES = $$SOURCES
SOURCES =
hook_compiler.name = hook ${QMAKE_FILE_IN}
hook_compiler.input = REAL_SOURCES
hook_compiler.variable_out = SOURCES
hook_compiler.commands = $$QPMX_BIN hook --prepare ${QMAKE_FILE_IN} --out ${QMAKE_FILE_OUT}
hook_compiler.output = $$OUT_PWD/.srccache/${QMAKE_FILE_BASE}${QMAKE_FILE_EXT}
QMAKE_EXTRA_COMPILERS += hook_compiler

# resources
!isEmpty(RESOURCES): write_file($$OUT_PWD/.qpmx_resources, RESOURCES)

# install stuff
isEmpty(PUBLIC_HEADERS): PUBLIC_HEADERS = $$HEADERS

target.path = $$QPMX_INSTALL/lib
headers.files = $$PUBLIC_HEADERS
headers.path = $$QPMX_INSTALL/include

INSTALLS += headers
isEmpty(REAL_SOURCES):isEmpty(GENERATED_SOURCES):write_file($$OUT_PWD/.no_sources_detected)
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
	ts_install.files += "$$OUT_PWD/$$replace(tsBase, \.ts, .qm)"
}
INSTALLS += ts_install
