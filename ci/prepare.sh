#!/bin/bash
set -e

./qtmodules-travis/ci/linux/build-all-orig.sh
./ci/travis_test.sh
