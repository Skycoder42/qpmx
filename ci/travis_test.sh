#!/bin/bash
# $1 sudo
set -e

SUDO=$1

export MAKEFLAGS="-j$(nproc)"
export PATH="$PATH:$(pwd)/build-$PLATFORM/qpmx"

# install plugins into qt
$SUDO mkdir /opt/qt/$QT_VER/$PLATFORM/plugins/qpmx
$SUDO cp plugins/qpmx/* /opt/qt/$QT_VER/$PLATFORM/plugins/qpmx/

mkdir build-$PLATFORM/tests
cd build-$PLATFORM/tests

set +e
/opt/qt/$QT_VER/$PLATFORM/bin/qmake -r $QMAKE_FLAGS ../../submodules/qpmx-sample-package/qpmx-test/

for log in $(find /var/folders/bb/ -iname "*qpmx*"); do
	cat $log/qmake.stdout.log
	cat $log/qmake.stderr.log
	cat $log/make.stdout.log
	cat $log/make.stderr.log
	cat $log/install.stdout.log
	cat $log/install.stderr.log
done

make
QT_QPA_PLATFORM=minimal ./test
