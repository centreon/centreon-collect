#!/bin/sh

set -e
set -x

# Out-of-source build.
rm -rf /tmp/build
mkdir /tmp/build
cd /tmp/build

# Configure project.
CXXFLAGS="-std=c++11" cmake -DWITH_TESTING=1 /usr/local/src/centreon-engine

# Build project.
make -j 4

# Run unit tests.
./tests/ut --gtest_output=xml:/tmp/ut.xml
