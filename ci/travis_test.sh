#!/bin/bash
# $1 sudo
set -e

SUDO=$1
unset QPMX_CACHE_DIR

if [[ "$PLATFORM" == "clang_64" ]]; then
	export MAKEFLAGS="-j$(sysctl -n hw.ncpu)"
	sudo mkdir -p /private || true
	sudo ln -s /tmp /private/tmp || true
	sudo ln -s /Users /private/Users || true
else
	export MAKEFLAGS="-j$(nproc)"
fi

export PATH="$PWD/build-$PLATFORM/qpmx:$PATH"
export QT_PLUGIN_PATH="$PWD/build-$PLATFORM/plugins:$QT_PLUGIN_PATH"
export LD_LIBRARY_PATH="$PWD/build-$PLATFORM/lib:$LD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH="$PWD/build-$PLATFORM/lib:$DYLD_LIBRARY_PATH"
which qpmx
if [[ "$(which qpmx)" != "$PWD/build-$PLATFORM/qpmx/qpmx" ]]; then
	echo wrong qpmx executable found
	exit 1
fi
qpmx list providers

# install plugins into qt
$SUDO mkdir /opt/qt/$QT_VER/$PLATFORM/plugins/qpmx
$SUDO cp build-$PLATFORM/plugins/qpmx/* /opt/qt/$QT_VER/$PLATFORM/plugins/qpmx/

# build tests (bin and src)
for i in 0 1 2 3; do #compile, compile-dev, src-dev, src
	if [[ "$i" == "1" ]]; then
		mv submodules/qpmx-sample-package/qpmx-test/qpmx.json.user.cm submodules/qpmx-sample-package/qpmx-test/qpmx.json.user
	fi
	if [[ "$i" == "2" ]]; then
		rm submodules/qpmx-sample-package/qpmx-test/qpmx.json
		mv submodules/qpmx-sample-package/qpmx-test/qpmx.json.src submodules/qpmx-sample-package/qpmx-test/qpmx.json
	fi
	if [[ "$i" == "3" ]]; then
		rm submodules/qpmx-sample-package/qpmx-test/qpmx.json.user
	fi

	for j in 0 1 2; do
		echo running test case $i-$j
		
		M_FLAGS="$QMAKE_FLAGS"
		if [[ "$j" == "1" ]]; then
			M_FLAGS="$QMAKE_FLAGS CONFIG+=test_as_shared"
		fi
		if [[ "$j" == "2" ]]; then
			M_FLAGS="$QMAKE_FLAGS CONFIG+=test_as_static"
		fi
		
		mkdir build-$PLATFORM/tests-$i-$j
		pushd build-$PLATFORM/tests-$i-$j

		/opt/qt/$QT_VER/$PLATFORM/bin/qmake $M_FLAGS ../../submodules/qpmx-sample-package/qpmx-test/
		make qmake_all
		make
		make lrelease
		make INSTALL_ROOT=$(mktemp -d) install

		if [[ "$j" == "0" ]]; then
			QT_QPA_PLATFORM=minimal ./test
		fi
		popd
	done
done

#extra tests
#test install without provider/version
qpmx install -cr --verbose de.skycoder42.qpathedit https://github.com/Skycoder42/qpmx-sample-package.git
