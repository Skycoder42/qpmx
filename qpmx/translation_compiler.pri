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

qpmx_translate_c.name = qpmx translate ${QMAKE_FILE_IN}
qpmx_translate_c.input = TRANSLATIONS
qpmx_translate_c.variable_out = DISTFILES
qpmx_translate_c.commands = qpmx translate $$QPMX_EXTRA_OPTIONS --outdir $$shell_quote($$QPMX_TRANSLATE_DIR) --qpmx $$shell_quote($$PWD/.qpmx.cache) --ts-file ${QMAKE_FILE_IN}
qpmx_src_build: qpmx_translate_c.commands += $$QPMX_LRELEASE %% $$QPMX_TRANSLATIONS
else: qpmx_translate_c.commands += --qmake $$shell_quote($$QMAKE_QMAKE) --lconvert $$shell_quote($$QPMX_LCONVERT) $$QPMX_LRELEASE
qpmx_translate_c.output = $$QPMX_TRANSLATE_DIR/${QMAKE_FILE_BASE}.qm
qpmx_translate_c.clean += $$QPMX_TRANSLATE_DIR/${QMAKE_FILE_BASE}.qm-base
qpmx_translate_c.CONFIG += target_predeps no_link

QMAKE_EXTRA_COMPILERS += qpmx_translate_c

QMAKE_DIR_REPLACE += QPMX_TRANSLATE_DIR
QMAKE_DIR_REPLACE_SANE += QPMX_TRANSLATE_DIR
