#!/bin/sh

set -e
set -x

# Install Centreon Broker headers.
yum install --nogpgcheck -y centreon-broker-devel

# Out-of-source build.
rm -rf /tmp/build
mkdir /tmp/build
cd /tmp/build

# Configure project.
cmake -DWITH_TESTING=1 /usr/local/src/centreon-discovery-engine/build

# Build project.
make -j 4

# Run unit tests.
#./tests/ut --gtest_output=xml:/tmp/ut.xml
