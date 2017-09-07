TEMPLATE = app

QT += core jsonserializer
QT -= gui

CONFIG += c++11 onsole
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

include(../submodules/submodules.pri)

PUBLIC_HEADERS += \
	sourceplugin.h \
	packageinfo.h

HEADERS += $$PUBLIC_HEADERS \
	installcommand.h \
	command.h \
	pluginregistry.h \
	qpmxformat.h \
	listcommand.h \
	compilecommand.h

SOURCES += main.cpp \
	installcommand.cpp \
	command.cpp \
	pluginregistry.cpp \
	qpmxformat.cpp \
	listcommand.cpp \
	compilecommand.cpp

unix {
	target.path = $$[QT_INSTALL_BINS]

	tHeaders.path = $$[QT_INSTALL_HEADERS]/../qpmx
	tHeaders.files = $$PUBLIC_HEADERS

	priBase.path = $$[QT_INSTALL_DATA]/../qpmx
	priBase.files = qpmx.pri

	INSTALLS += target tHeaders priBase
}

RESOURCES += \
	qpmx.qrc
