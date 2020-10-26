#!/bin/sh

set -e
set -x

# Out-of-source build.
rm -rf /tmp/build
mkdir /tmp/build
cd /tmp/build

# Configure project.
if [ -e /usr/bin/cmake3 ] ; then
  mycmake=cmake3
else
  mycmake=cmake
fi
conan install /usr/local/src/centreon-engine
CXXFLAGS="-std=c++11" $mycmake -DWITH_TESTING=1 /usr/local/src/centreon-engine

# Build project.
make -j 4

# Run unit tests.
./tests/ut --gtest_output=xml:/tmp/ut.xml
