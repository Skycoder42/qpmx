#!/bin/bash
# $1 sudo
set -e

SUDO=$1

if [[ "$PLATFORM" == "clang_64" ]]; then
	export MAKEFLAGS="-j$(sysctl -n hw.ncpu)"
else
	export MAKEFLAGS="-j$(nproc)"
fi
export PATH="$PATH:$(pwd)/build-$PLATFORM/qpmx"
which qpmx

# install plugins into qt
$SUDO mkdir /opt/qt/$QT_VER/$PLATFORM/plugins/qpmx
$SUDO cp build-$PLATFORM/plugins/qpmx/* /opt/qt/$QT_VER/$PLATFORM/plugins/qpmx/

# build tests (bin and src)
for i in 0 1 2; do
	if [[ "$i" == "1" ]]; then
		mv submodules/qpmx-sample-package/qpmx-test/qpmx.json.user.cm submodules/qpmx-sample-package/qpmx-test/qpmx.json.user
	fi
	if [[ "$i" == "2" ]]; then
		rm submodules/qpmx-sample-package/qpmx-test/qpmx.json
		rm submodules/qpmx-sample-package/qpmx-test/qpmx.json.user
		mv submodules/qpmx-sample-package/qpmx-test/qpmx.json.src submodules/qpmx-sample-package/qpmx-test/qpmx.json
	fi

	mkdir build-$PLATFORM/tests-$i
	pushd build-$PLATFORM/tests-$i

	/opt/qt/$QT_VER/$PLATFORM/bin/qmake -r $QMAKE_FLAGS ../../submodules/qpmx-sample-package/qpmx-test/
	make
	make lrelease
	make INSTALL_ROOT=$(mktemp -d) install

	QT_QPA_PLATFORM=minimal ./test
	popd
done

#debug
build-$PLATFORM/qpmx/qtifw-installer/packages/de.skycoder42.qpmx/data/qpmx list providers
