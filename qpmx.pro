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
	qpmx_fix_target.target = $$INSTALL_BINS/$${PROJECT_TARGET}
	qpmx_fix_target.name = $${PROJECT_TARGET}
	qpmx_fix_target.version = 1
	BUNDLE_EXTRA_LIBS += qpmx_fix_target

	install.commands += install_name_tool -add_rpath "@executable_path/../lib" "$(INSTALL_ROOT)$${qpmx_fix_target.target}"$$escape_expand(\n\t)
}

include(submodules/deployment/deploy.pri)
