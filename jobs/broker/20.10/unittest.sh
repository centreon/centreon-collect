#!/bin/sh

set -e
set -x

# Out-of-source build.
rm -rf /tmp/build
mkdir /tmp/build
cd /tmp/build

# Configure project.
conan install /usr/local/src/centreon-broker
CXXFLAGS="-O0 -g3 -std=c++11 -Wall -Wno-long-long" cmake3 -DWITH_TESTING=1 /usr/local/src/centreon-broker

# Build project.
make -j 4

# Run unit tests.
./test/ut --gtest_output=xml:/tmp/ut.xml
