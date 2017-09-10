#!/bin/bash
set -e

apt-get -qq update
apt-get install software-properties-common
add-apt-repository --yes ppa:ubuntu-toolchain-r/test
apt-get -qq update
apt-get -qq install --no-install-recommends g++-7
