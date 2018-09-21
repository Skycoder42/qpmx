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
