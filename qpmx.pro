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
win32: DEPLOY_BINS = "$$INSTALL_BINS/$${PROJECT_TARGET}.exe"
!win32: CONFIG += no_deploy_qt_qm
include(submodules/deployment/deploy.pri)
