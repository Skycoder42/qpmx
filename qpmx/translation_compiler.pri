isEmpty(QPMX_TRANSLATE_DIR):QPMX_TRANSLATE_DIR = .
debug_and_release {
	CONFIG(debug, debug|release):SUFFIX = /debug
	CONFIG(release, debug|release):SUFFIX = /release
	QPMX_TRANSLATE_DIR = $$QPMX_TRANSLATE_DIR$$SUFFIX
}

isEmpty(QPMX_LRELEASE) {
	isEmpty(LRELEASE): qtPrepareTool(LRELEASE, lrelease)
	QPMX_LRELEASE = $$replace(LRELEASE, -, +)
}
!qpmx_src_build: qtPrepareTool(QPMX_LCONVERT, lconvert)

win32: QPMX_SRC_SEPERATOR = %%%%
else: QPMX_SRC_SEPERATOR = %%
qpmx_translate.name = qpmx translate ${QMAKE_FILE_IN}
qpmx_translate.input = TRANSLATIONS
qpmx_translate.variable_out = DISTFILES
qpmx_translate.commands = qpmx translate $$QPMX_TRANSLATE_EXTRA_OPTIONS --outdir $$shell_quote($$QPMX_TRANSLATE_DIR) --qpmx $$shell_quote($$PWD/.qpmx.cache) --ts-file ${QMAKE_FILE_IN}
qpmx_src_build:win32: qpmx_translate.commands += $$QPMX_LRELEASE $$QPMX_SRC_SEPERATOR $$QPMX_TRANSLATIONS
else: qpmx_translate.commands += --qmake $$shell_quote($$QMAKE_QMAKE) --lconvert $$shell_quote($$QPMX_LCONVERT) $$QPMX_LRELEASE
qpmx_translate.output = $$QPMX_TRANSLATE_DIR/${QMAKE_FILE_BASE}.qm
qpmx_translate.clean += $$QPMX_TRANSLATE_DIR/${QMAKE_FILE_BASE}.qm-base
qpmx_translate.CONFIG += no_link #target_predeps

QMAKE_EXTRA_COMPILERS += qpmx_translate

QMAKE_DIR_REPLACE += QPMX_TRANSLATE_DIR
QMAKE_DIR_REPLACE_SANE += QPMX_TRANSLATE_DIR

lreleaseTarget.target = lrelease
lreleaseTarget.depends += compiler_qpmx_translate_make_all
QMAKE_EXTRA_TARGETS += lreleaseTarget
