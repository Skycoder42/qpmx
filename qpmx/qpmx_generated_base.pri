#setup
isEmpty(QPMX_WORKINGDIR):QPMX_WORKINGDIR = .
debug_and_release {
	CONFIG(debug, debug|release): SUFFIX = /debug
	CONFIG(release, debug|release): SUFFIX = /release
	QPMX_WORKINGDIR = $$QPMX_WORKINGDIR$$SUFFIX
}

win32: QPMX_SRC_SEPERATOR = %%%%
else: QPMX_SRC_SEPERATOR = %%

qpmx_src_build:CONFIG(static, static|dynamic): warning(qpmx source builds cannot generate a static library, as startup hooks and resources will not be available. Please switch to a compiled qpmx build!)

#qpmx startup hook
!qpmx_src_build:!isEmpty(QPMX_STARTUP_HOOKS)|!isEmpty(QPMX_RESOURCE_FILES) {
	qpmx_hook_target.target = "$$QPMX_WORKINGDIR/qpmx_startup_hooks.cpp"
	qpmx_hook_target.commands = $$QPMX_BIN hook $$QPMX_HOOK_EXTRA_OPTIONS --out $$shell_quote($$QPMX_WORKINGDIR/qpmx_startup_hooks.cpp) $$QPMX_STARTUP_HOOKS $$QPMX_SRC_SEPERATOR $$QPMX_RESOURCE_FILES
	qpmx_hook_target.depends += $$PWD/qpmx_generated.pri
	QMAKE_EXTRA_TARGETS += qpmx_hook_target
	GENERATED_SOURCES += "$$QPMX_WORKINGDIR/qpmx_startup_hooks.cpp"

	qpmx_hook_target_clean.target = qpmx-create-hooks-clean
	qpmx_hook_target_clean.commands = $$QMAKE_DEL_FILE $$shell_quote($$shell_path($$QPMX_WORKINGDIR/qpmx_startup_hooks.cpp))
	clean.depends += qpmx_hook_target_clean
	QMAKE_EXTRA_TARGETS += qpmx_hook_target_clean clean
}

#translations
QPMX_TRANSLATIONS = $$TRANSLATIONS #translations comming from the qpmx dependencies (src only)
TRANSLATIONS = $$QPMX_TMP_TS
isEmpty(QPMX_LRELEASE) {
	isEmpty(LRELEASE): qtPrepareTool(LRELEASE, lrelease)
	QPMX_LRELEASE = $$replace(LRELEASE, -, +)
}
!qpmx_src_build: qtPrepareTool(QPMX_LCONVERT, lconvert)

qpmx_translate.name = $$QPMX_BIN translate ${QMAKE_FILE_IN}
qpmx_translate.input = TRANSLATIONS
qpmx_translate.variable_out = TRANSLATIONS_QM
qpmx_translate.commands = $$QPMX_BIN translate $$QPMX_TRANSLATE_EXTRA_OPTIONS --outdir $$shell_quote($$QPMX_WORKINGDIR) --ts-file ${QMAKE_FILE_IN}
qpmx_src_build: qpmx_translate.commands += --src $$QPMX_LRELEASE $$QPMX_SRC_SEPERATOR $$QPMX_TRANSLATIONS
else: qpmx_translate.commands += --qmake $$shell_quote($$QMAKE_QMAKE) --lconvert $$shell_quote($$QPMX_LCONVERT) $$QPMX_LRELEASE $$QPMX_SRC_SEPERATOR $$QPMX_TS_DIRS
qpmx_translate.output = $$QPMX_WORKINGDIR/${QMAKE_FILE_BASE}.qm
qpmx_translate.clean += $$QPMX_WORKINGDIR/${QMAKE_FILE_BASE}.qm $$QPMX_WORKINGDIR/${QMAKE_FILE_BASE}.qm-base
qpmx_translate.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += qpmx_translate

qpmx_extra_translate.name = $$LRELEASE translate ${QMAKE_FILE_IN}
qpmx_extra_translate.input = EXTRA_TRANSLATIONS
qpmx_extra_translate.variable_out = TRANSLATIONS_QM
qpmx_extra_translate.commands = $$LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
qpmx_extra_translate.output = $$OUT_PWD/${QMAKE_FILE_BASE}.qm
qpmx_extra_translate.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += qpmx_extra_translate

lreleaseTarget.target = lrelease
win32:!ReleaseBuild:!DebugBuild: {
	lreleaseSubTarget.target = lrelease-subtarget
	lreleaseSubTarget.CONFIG += recursive
	lreleaseSubTarget.recurse_target = lrelease
	QMAKE_EXTRA_TARGETS += lreleaseSubTarget

	CONFIG(debug, debug|release): lreleaseTarget.depends += debug-lreleaseSubTarget
	CONFIG(release, debug|release): lreleaseTarget.depends += release-lreleaseSubTarget
} else: lreleaseTarget.depends += compiler_qpmx_translate_make_all compiler_qpmx_extra_translate_make_all
lreleaseTarget.commands =
QMAKE_EXTRA_TARGETS += lreleaseTarget

qpmx_ts_target.CONFIG += no_check_exist
#qpmx_ts_target.files = $$TRANSLATIONS_QM
for(tsfile, TRANSLATIONS) {
	tsBase = $$basename(tsfile)
	qpmx_ts_target.files += "$$OUT_PWD/$$QPMX_WORKINGDIR/$$replace(tsBase, \.ts, .qm)"
}
for(tsfile, EXTRA_TRANSLATIONS) {
	tsBase = $$basename(tsfile)
	qpmx_ts_target.files += "$$OUT_PWD/$$QPMX_WORKINGDIR/$$replace(tsBase, \.ts, .qm)"
}

QMAKE_DIR_REPLACE += QPMX_WORKINGDIR
QMAKE_DIR_REPLACE_SANE += QPMX_WORKINGDIR
