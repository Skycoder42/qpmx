#!/bin/bash
set -e

add-apt-repository --yes ppa:ubuntu-toolchain-r/test
apt-get -qq update
apt-get -qq install --no-install-recommends g++-7
