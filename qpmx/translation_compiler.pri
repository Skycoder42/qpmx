isEmpty(QPMX_TRANSLATE_DIR):QPMX_TRANSLATE_DIR = .
debug_and_release {
	CONFIG(debug, debug|release): SUFFIX = /debug
	CONFIG(release, debug|release): SUFFIX = /release
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
qpmx_translate.clean += $$QPMX_TRANSLATE_DIR/${QMAKE_FILE_BASE}.qm-base #TODO clean qm files aswell?
qpmx_translate.CONFIG += no_link #target_predeps

QMAKE_EXTRA_COMPILERS += qpmx_translate

lreleaseTarget.target = lrelease
win32:!ReleaseBuild:!DebugBuild: {
	lreleaseSubTarget.target = lrelease-subtarget
	lreleaseSubTarget.CONFIG += recursive
	lreleaseSubTarget.recurse_target = lrelease
	QMAKE_EXTRA_TARGETS += lreleaseSubTarget

	CONFIG(debug, debug|release): lreleaseTarget.depends += debug-lreleaseSubTarget
	CONFIG(release, debug|release): lreleaseTarget.depends += release-lreleaseSubTarget
} else: lreleaseTarget.depends += compiler_qpmx_translate_make_all
lreleaseTarget.commands =
QMAKE_EXTRA_TARGETS += lreleaseTarget

qpmx_ts_target.CONFIG += no_check_exist
#qpmx_ts_target.files = $$TRANSLATIONS_QM
for(tsfile, TRANSLATIONS) {
	tsBase = $$basename(tsfile)
	qpmx_ts_target.files += "$$OUT_PWD/$$QPMX_TRANSLATE_DIR/$$replace(tsBase, .ts, .qm)"
}

QMAKE_DIR_REPLACE += QPMX_TRANSLATE_DIR
QMAKE_DIR_REPLACE_SANE += QPMX_TRANSLATE_DIR
