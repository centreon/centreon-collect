#!/bin/sh

set -e
set -x

# Out-of-source build.
rm -rf /tmp/build
mkdir /tmp/build
cd /tmp/build

# Configure project.
CXXFLAGS="-O0 -g3 -std=c++11 -Wall -Wno-long-long" cmake -DWITH_TESTING=1 /usr/local/src/centreon-broker/build

# Build project.
make -j 4

# Run unit tests.
./tests/ut --gtest_output=xml:/tmp/ut.xml
