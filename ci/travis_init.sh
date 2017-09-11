#!/bin/bash
set -e

currDir=$(dirname $0)

# move data into docker
if [[ $PLATFORM == "gcc_64" ]]; then
	#echo 'QMAKE_CXX=g++-6' >> .qmake.conf
	mv ./qtmodules-travis/ci/linux/build-all.sh ./qtmodules-travis/ci/linux/build-all-orig.sh
	mv $currDir/prepare.sh ./qtmodules-travis/ci/linux/build-all.sh
fi
