#!/bin/bash
# $1 sudo
set -e

SUDO=$1

export MAKEFLAGS="-j$(nproc)"
export PATH="$PATH:$(pwd)/build-$PLATFORM/qpmx"

# install plugins into qt
pushd build-$PLATFORM/plugins
$SUDO make install
popd

mkdir build-$PLATFORM/tests
cd build-$PLATFORM/tests

set +e
/opt/qt/$QT_VER/$PLATFORM/bin/qmake -r $QMAKE_FLAGS ../../submodules/qpmx-sample-package/qpmx-test/ || true


echo after qmake

find /var/folders/bb/ -iname "*qpmx*"
dir /var/folders/bb/*/T/qpmx-Bmjbnz
cat /var/folders/bb/*/T/qpmx-Bmjbnz/make.stderr.log

for log in $(find /var/folders/bb/ -iname "*qpmx*"); do
	cat $log/make.stderr.log
done



make

QT_QPA_PLATFORM=minimal ./test
