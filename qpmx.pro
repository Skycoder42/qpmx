TEMPLATE = subdirs

SUBDIRS += \
	qpmx \
	plugins \
	lib

qpmx.depends += lib
plugins.depends += lib

DISTFILES += .qmake.conf \
	README.md

QMAKE_EXTRA_TARGETS += lrelease

CONFIG += install_log
include(submodules/deployment/install.pri)

win32: DEPLOY_BINS = "$$INSTALL_BINS/$${PROJECT_TARGET}.exe"
!win32: CONFIG += no_deploy_qt_qm

mac {
	qpmx_fix_bin.target = $$INSTALL_BINS/$${PROJECT_TARGET}
	qpmx_fix_bin.name = $${PROJECT_TARGET}
	qpmx_fix_bin.version = 1
	qpmx_fix_git.target = $$INSTALL_PLUGINS/qpmx/libgitsource.dylib
	qpmx_fix_git.name = $${PROJECT_TARGET}
	qpmx_fix_git.version = 1
	qpmx_fix_qpm.target = $$INSTALL_PLUGINS/qpmx/libqpmsource.dylib
	qpmx_fix_qpm.name = $${PROJECT_TARGET}
	qpmx_fix_qpm.version = 1
	BUNDLE_EXTRA_LIBS += qpmx_fix_bin qpmx_fix_qpm qpmx_fix_git

	install.commands += install_name_tool -add_rpath "@executable_path/../lib" "$(INSTALL_ROOT)$${qpmx_fix_bin.target}"$$escape_expand(\n\t)
}

include(submodules/deployment/deploy.pri)
