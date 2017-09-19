TEMPLATE = app

QT += core jsonserializer
QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

TARGET = qpmx
VERSION = $$QPMXVER

QMAKE_TARGET_COMPANY = "Skycoder42"
QMAKE_TARGET_PRODUCT = $$TARGET
QMAKE_TARGET_DESCRIPTION = "qpmx package manager"
QMAKE_TARGET_COPYRIGHT = "Felix Barz"
QMAKE_TARGET_BUNDLE_PREFIX = de.skycoder42

DEFINES += "TARGET=\\\"$$TARGET\\\""
DEFINES += "VERSION=\\\"$$VERSION\\\""
DEFINES += "COMPANY=\"\\\"$$QMAKE_TARGET_COMPANY\\\"\""
DEFINES += "BUNDLE=\"\\\"$$QMAKE_TARGET_BUNDLE_PREFIX\\\"\""

PUBLIC_HEADERS += \
	sourceplugin.h \
	packageinfo.h

HEADERS += $$PUBLIC_HEADERS \
	installcommand.h \
	command.h \
	pluginregistry.h \
	qpmxformat.h \
	listcommand.h \
	compilecommand.h \
	generatecommand.h \
	topsort.h \
	searchcommand.h \
	uninstallcommand.h \
	initcommand.h \
    translatecommand.h

SOURCES += main.cpp \
	installcommand.cpp \
	command.cpp \
	pluginregistry.cpp \
	qpmxformat.cpp \
	listcommand.cpp \
	compilecommand.cpp \
	generatecommand.cpp \
	searchcommand.cpp \
	uninstallcommand.cpp \
	initcommand.cpp \
    translatecommand.cpp

RESOURCES += \
	qpmx.qrc

DISTFILES += \
	config.xml \
	meta/*

no_installer {
	target.path = $$[QT_INSTALL_BINS]

	tHeaders.path = $$[QT_INSTALL_HEADERS]/../qpmx
	tHeaders.files = $$PUBLIC_HEADERS

	INSTALLS += target tHeaders
} else { # installer
	QTIFW_CONFIG = config.xml
	QTIFW_MODE = online_all

	mac: subdir1.name = Headers
	else: subdir1.name = include
	subdir1.files += $$PUBLIC_HEADERS

	mac: subdir2.name = PlugIns/qpmx
	else: subdir2.name = plugins/qpmx
	subdir2.dirs += $$OUT_PWD/../plugins/qpmx

	qpmxpkg.pkg = de.skycoder42.qpmx
	qpmxpkg.meta += meta
	qpmxpkg.subdirs += subdir1 subdir2
	QTIFW_AUTO_INSTALL_PKG = qpmxpkg

	QTIFW_PACKAGES += qpmxpkg
	QTIFW_DEPLOY_TSPRO = $$_PRO_FILE_

	CONFIG += qtifw_auto_deploy
	CONFIG += qtifw_install_target
}

include(../submodules/submodules.pri)
