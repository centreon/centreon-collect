#!/bin/sh

set -e
set -x

# Out-of-source build.
rm -rf /tmp/build
mkdir /tmp/build
cd /tmp/build

# Configure project.
case "$1" in
  "centos6")
    cmake -DWITH_TESTING=1 /usr/local/src/centreon-engine
    ;;
  *)
    CXXFLAGS="-std=c++11" cmake -DWITH_TESTING=1 /usr/local/src/centreon-engine
    ;;
esac

# Build project.
make -j 4

# Run unit tests.
./tests/ut --gtest_output=xml:/tmp/ut.xml
