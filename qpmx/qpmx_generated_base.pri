#setup
isEmpty(QPMX_WORKINGDIR):QPMX_WORKINGDIR = .
debug_and_release {
	CONFIG(debug, debug|release): SUFFIX = /debug
	CONFIG(release, debug|release): SUFFIX = /release
	QPMX_WORKINGDIR = $$QPMX_WORKINGDIR$$SUFFIX
}

#qpmx startup hook
!isEmpty(QPMX_STARTUP_HASHES) {
	qpmx_hook_target.target = "$$QPMX_WORKINGDIR/qpmx_startup_hooks.cpp"
	qpmx_hook_target.commands = qpmx hook $$QPMX_HOOK_EXTRA_OPTIONS --out $$shell_quote($$QPMX_WORKINGDIR/qpmx_startup_hooks.cpp) $$QPMX_STARTUP_HASHES
	qpmx_hook_target.depends += $$PWD/qpmx_generated.pri
	qpmx_hook_target_clean.target = qpmx-create-hooks-clean
	qpmx_hook_target_clean.commands = $$QMAKE_DEL_FILE $$shell_quote($$shell_path($$QPMX_WORKINGDIR/qpmx_startup_hooks.cpp))
	clean.depends += qpmx_hook_target_clean
	QMAKE_EXTRA_TARGETS += qpmx_hook_target qpmx_hook_target_clean clean
	GENERATED_SOURCES += "$$QPMX_WORKINGDIR/qpmx_startup_hooks.cpp"
}

#translations
TRANSLATIONS = $$QPMX_TMP_TS
isEmpty(QPMX_LRELEASE) {
	isEmpty(LRELEASE): qtPrepareTool(LRELEASE, lrelease)
	QPMX_LRELEASE = $$replace(LRELEASE, -, +)
}
!qpmx_src_build: qtPrepareTool(QPMX_LCONVERT, lconvert)

win32: QPMX_SRC_SEPERATOR = %%%%
else: QPMX_SRC_SEPERATOR = %%
qpmx_translate.name = qpmx translate ${QMAKE_FILE_IN}
qpmx_translate.input = TRANSLATIONS
qpmx_translate.variable_out = DISTFILES #TRANSLATIONS_QM
qpmx_translate.commands = qpmx translate $$QPMX_TRANSLATE_EXTRA_OPTIONS --outdir $$shell_quote($$QPMX_WORKINGDIR) --qpmx $$shell_quote($$PWD/.qpmx.cache) --ts-file ${QMAKE_FILE_IN}
qpmx_src_build:win32: qpmx_translate.commands += $$QPMX_LRELEASE $$QPMX_SRC_SEPERATOR $$QPMX_TRANSLATIONS
else: qpmx_translate.commands += --qmake $$shell_quote($$QMAKE_QMAKE) --lconvert $$shell_quote($$QPMX_LCONVERT) $$QPMX_LRELEASE
qpmx_translate.output = $$QPMX_WORKINGDIR/${QMAKE_FILE_BASE}.qm
qpmx_translate.clean += $$QPMX_WORKINGDIR/${QMAKE_FILE_BASE}.qm $$QPMX_WORKINGDIR/${QMAKE_FILE_BASE}.qm-base
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
	qpmx_ts_target.files += "$$OUT_PWD/$$QPMX_WORKINGDIR/$$replace(tsBase, .ts, .qm)"
}

QMAKE_DIR_REPLACE += QPMX_WORKINGDIR
QMAKE_DIR_REPLACE_SANE += QPMX_WORKINGDIR
