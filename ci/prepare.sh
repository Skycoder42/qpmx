#!/bin/bash
set -e

apt-get -qq update
apt-get -qq install --no-install-recommends software-properties-common
add-apt-repository --yes ppa:ubuntu-toolchain-r/test
apt-get -qq update
apt-get -qq install --no-install-recommends g++-7

./qtmodules-travis/ci/linux/build-all-orig.sh
