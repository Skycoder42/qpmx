#setup
isEmpty(QPMX_WORKINGDIR):QPMX_WORKINGDIR = .
debug_and_release {
	CONFIG(debug, debug|release): SUFFIX = /debug
	CONFIG(release, debug|release): SUFFIX = /release
	QPMX_WORKINGDIR = $$QPMX_WORKINGDIR$$SUFFIX
}

# libbuild detection
PRE_TARGETDEPS += $$QPMX_LIB_DEPS #lib targetdeps, needed for private merge
!qpmx_no_libbuild:equals(TEMPLATE, lib) {
	CONFIG += qpmx_as_private_lib
	CONFIG(static, static|shared):!isEmpty(QPMX_LIB_DEPS) {
		qpmx_lib_merge.target = qpmx_lib_merge
		debug_and_release: QPMX_MERGE_TARGET = $(DESTDIR_TARGET)
		else: QPMX_MERGE_TARGET = $(TARGET)

		mac|ios|win32:!mingw {
			QPMX_RAW_TARGET = $${QPMX_MERGE_TARGET}.raw
			QPMX_LIB_DEPS += $$QPMX_RAW_TARGET
			qpmx_lib_merge.commands = $$QMAKE_MOVE $$QPMX_MERGE_TARGET $$QPMX_RAW_TARGET $$escape_expand(\\n\\t)
			mac|ios: qpmx_lib_merge.commands += libtool -static -o $$QPMX_MERGE_TARGET $$QPMX_LIB_DEPS $$escape_expand(\\n\\t)
			win32: qpmx_lib_merge.commands += lib.exe /OUT:$$QPMX_MERGE_TARGET $$QPMX_LIB_DEPS $$escape_expand(\\n\\t)
			qpmx_lib_merge.depends += "$$QPMX_MERGE_TARGET"
		} else {
			qpmx_lib_mri.target = $${QPMX_MERGE_TARGET}.mri
			qpmx_lib_mri.commands = echo "OPEN $${QPMX_MERGE_TARGET}" > $${QPMX_MERGE_TARGET}.mri $$escape_expand(\\n\\t)
			for(lib, QPMX_LIB_DEPS): qpmx_lib_mri.commands += echo "addlib $$lib" >> $${QPMX_MERGE_TARGET}.mri $$escape_expand(\\n\\t)
			qpmx_lib_mri.commands += echo "save" >> $${QPMX_MERGE_TARGET}.mri $$escape_expand(\\n\\t)
			qpmx_lib_mri.commands += echo "end" >> $${QPMX_MERGE_TARGET}.mri
			qpmx_lib_merge.commands += ar -M < "$${QPMX_MERGE_TARGET}.mri" $$escape_expand(\\n\\t)
			qpmx_lib_merge.depends += "$${QPMX_MERGE_TARGET}" qpmx_lib_mri
			QMAKE_EXTRA_TARGETS += qpmx_lib_mri

			qpmx_lib_mri_clean.target = qpmx_lib_mri_clean
			qpmx_lib_mri_clean.commands = $$QMAKE_DEL_FILE $${QPMX_MERGE_TARGET}.mri
			clean.depends += qpmx_lib_mri_clean
			QMAKE_EXTRA_TARGETS += qpmx_lib_mri_clean clean
		}

		qpmx_lib_merge.commands += echo "merged" > qpmx_lib_merge #prevents the target from beeing run multiple times
		qpmx_lib_merge_clean.target = qpmx_lib_merge_clean
		qpmx_lib_merge_clean.commands = $$QMAKE_DEL_FILE qpmx_lib_merge
		clean.depends += qpmx_lib_merge_clean

		all.depends += qpmx_lib_merge
		staticlib.depends += qpmx_lib_merge
		debug_and_release:!ReleaseBuild:!DebugBuild {
			qpmx_lib_merge_sub.target = qpmx_lib_merge
			qpmx_lib_merge_sub.CONFIG += recursive
			qpmx_lib_merge_sub.recurse_target = qpmx_lib_merge
			QMAKE_EXTRA_TARGETS += qpmx_lib_merge_sub
		} else: QMAKE_EXTRA_TARGETS += qpmx_lib_merge qpmx_lib_merge_clean all staticlib clean
	}
}

win32:!mingw: QPMX_SRC_SEPERATOR = %%%%
else: QPMX_SRC_SEPERATOR = %%

qpmx_src_build:CONFIG(static, static|shared): warning(qpmx source builds cannot generate a static library, as startup hooks and resources will not be available. Please switch to a compiled qpmx build!)

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
QPMX_TRANSLATIONS += $$TRANSLATIONS #translations comming from the qpmx dependencies (src only)
TRANSLATIONS = $$QPMX_TMP_TS
isEmpty(QPMX_LRELEASE) {
	isEmpty(LRELEASE): qtPrepareTool(LRELEASE, lrelease)
	for(arg, LRELEASE):	QPMX_LRELEASE += "+$${arg}"
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

lrelease_target.target = lrelease
debug_and_release:!ReleaseBuild:!DebugBuild: {
	lrelease_sub_target.target = lrelease-subtarget
	lrelease_sub_target.CONFIG += recursive
	lrelease_sub_target.recurse_target = lrelease
	QMAKE_EXTRA_TARGETS += lrelease_sub_target

	CONFIG(debug, debug|release): lrelease_target.depends += debug-lrelease_sub_target
	CONFIG(release, debug|release): lrelease_target.depends += release-lrelease_sub_target
} else: lrelease_target.depends += compiler_qpmx_translate_make_all compiler_qpmx_extra_translate_make_all
lrelease_target.commands =
QMAKE_EXTRA_TARGETS += lrelease_target

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
