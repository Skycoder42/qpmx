#!/bin/bash
set -e

./qtmodules-travis/ci/linux/build-docker-orig.sh
./ci/travis_test.sh
