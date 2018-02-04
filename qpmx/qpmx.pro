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
	translatecommand.h \
	devcommand.h \
	preparecommand.h \
	publishcommand.h \
	createcommand.h \
	hookcommand.h \
	clearcachescommand.h \
	updatecommand.h

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
	updatecommand.cpp

RESOURCES += \
	qpmx.qrc

DISTFILES += \
	config.xml \
	meta/* \
	qpmx

include(../submodules/qcliparser/qcliparser.pri)
include(../install.pri)

target.path = $$INSTALL_BINS
tHeaders.path = $${INSTALL_HEADERS}/qpmx
tHeaders.files = $$PUBLIC_HEADERS
INSTALLS += target tHeaders

unix {
	bashcomp.path = $${INSTALL_SHARE}/bash-completion/completions/
	bashcomp.files = qpmx
	INSTALLS += bashcomp
}

