TEMPLATE = subdirs

SUBDIRS += \
	qpmx \
	plugins

DISTFILES += .qmake.conf \
	README.md

QMAKE_EXTRA_TARGETS += lrelease
