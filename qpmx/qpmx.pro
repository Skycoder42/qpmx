TEMPLATE = app

QT += core
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
	sourceplugin.h

HEADERS += $$PUBLIC_HEADERS

SOURCES += main.cpp

unix {
	target.path = $$[QT_INSTALL_BINS]

	tHeaders.path = $$[QT_INSTALL_HEADERS]/../qpmx
	tHeaders.files = $$PUBLIC_HEADERS

	INSTALLS += target tHeaders
}
