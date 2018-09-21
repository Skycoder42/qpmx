TEMPLATE = app

QT += core jsonserializer
QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

TARGET = qpmx

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
	translatecommand.h \
	devcommand.h \
	preparecommand.h \
	publishcommand.h \
	createcommand.h \
	hookcommand.h \
	clearcachescommand.h \
	updatecommand.h \
	qbscommand.h

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
	translatecommand.cpp \
	devcommand.cpp \
	preparecommand.cpp \
	publishcommand.cpp \
	createcommand.cpp \
	hookcommand.cpp \
	clearcachescommand.cpp \
	updatecommand.cpp \
	qbscommand.cpp

RESOURCES += \
	qpmx.qrc

DISTFILES += \
	completitions/bash/qpmx \
	qbs/module.qbs \
	qbs/dep-base.qbs \
	qbs/MergedStaticLibrary.qbs

include(../submodules/qcliparser/qcliparser.pri)
include(../submodules/qpluginfactory/qpluginfactory.pri)
include(../lib.pri)
include(../install.pri)

target.path = $$INSTALL_BINS
tHeaders.path = $${INSTALL_HEADERS}/qpmx
tHeaders.files = $$PUBLIC_HEADERS
INSTALLS += target tHeaders

unix {
	bashcomp.path = $${INSTALL_SHARE}/bash-completion/completions/
	bashcomp.files = completitions/bash/qpmx
	zshcomp.path = $${INSTALL_SHARE}/zsh/site-functions/
	zshcomp.files = completitions/zsh/_qpmx
	INSTALLS += bashcomp zshcomp
}
