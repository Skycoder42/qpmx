#!/bin/bash
set -e

export MAKEFLAGS="-j$(nproc)"
export PATH="$PATH:$(pwd)/build-$PLATFORM/qpmx"

mkdir build-$PLATFORM/tests
cd build-$PLATFORM/tests

/opt/qt/$QT_VER/$PLATFORM/bin/qmake -r $QMAKE_FLAGS ../../submodules/qpmx-sample-package/qpmx-test/
make

QT_QPA_PLATFORM=minimal ./test
