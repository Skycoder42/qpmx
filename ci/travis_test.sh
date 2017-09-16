#!/bin/bash
# $1 sudo
set -e

SUDO=$1

if [[ "$PLATFORM" == "clang_64" ]]; then
	export MAKEFLAGS="-j$(sysctl -n hw.ncpu)"
	export PATH="$PATH:$(pwd)/build-$PLATFORM/qpmx.app/Contents/MacOS"
else
	export MAKEFLAGS="-j$(nproc)"
	export PATH="$PATH:$(pwd)/build-$PLATFORM/qpmx"
fi
which qpmx

# install plugins into qt
$SUDO mkdir /opt/qt/$QT_VER/$PLATFORM/plugins/qpmx
$SUDO cp build-$PLATFORM/plugins/qpmx/* /opt/qt/$QT_VER/$PLATFORM/plugins/qpmx/

mkdir build-$PLATFORM/tests
pushd build-$PLATFORM/tests

/opt/qt/$QT_VER/$PLATFORM/bin/qmake -r $QMAKE_FLAGS ../../submodules/qpmx-sample-package/qpmx-test/ || (
	res=$?
	for log in $(find /var/folders/bb/ -iname "*qpmx*"); do
		cat $log/qmake.stdout.log
		cat $log/qmake.stderr.log
		cat $log/make.stdout.log
		cat $log/make.stderr.log
		cat $log/install.stdout.log
		cat $log/install.stderr.log
	done
	exit $res
)

make
QT_QPA_PLATFORM=minimal ./test
popd

#debug
build-$PLATFORM/qpmx/qtifw-installer/packages/de.skycoder42.qpmx/data/qpmx --help
