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
qpmx_translate.variable_out = TRANSLATIONS_QM
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

#translations install target
contains(INSTALLS, qpmx_ts_target) {
	isEmpty(qpmx_ts_target.path): qpmx_ts_target.path = /

	qpmx_install_compiler.name = ts-install ${QMAKE_FILE_IN}
	qpmx_install_compiler.input = TRANSLATIONS_QM
	qpmx_install_compiler.commands = $$QMAKE_INSTALL_FILE ${QMAKE_FILE_IN} $(INSTALL_ROOT)${QMAKE_FILE_OUT}
	qpmx_install_compiler.output = $${qpmx_ts_target.path}/${QMAKE_FILE_BASE}.qm
	qpmx_install_compiler.CONFIG += no_link

	QMAKE_EXTRA_COMPILERS += qpmx_install_compiler

	qpmx_ts_target.target = qpmx_ts_install
	qpmx_ts_target.depends += compiler_qpmx_install_compiler_make_all
	qpmx_ts_target_dir.target = qpmx_ts_install_dir
	qpmx_ts_target_dir.commands = $$QMAKE_MKDIR $(INSTALL_ROOT)$${qpmx_ts_target.path} || true
	compiler_qpmx_install_compiler_make_all.depends += qpmx_ts_install_dir
	install.depends += qpmx_ts_install
	QMAKE_EXTRA_TARGETS += qpmx_ts_target qpmx_ts_target_dir install compiler_qpmx_install_compiler_make_all
}
