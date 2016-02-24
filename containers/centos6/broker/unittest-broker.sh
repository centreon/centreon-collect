#!/bin/sh

# Out-of-source build.
rm -rf /tmp/build
mkdir /tmp/build
cd /tmp/build

# Configure project.
cmake /usr/local/src/centreon-broker/build

# Build project.
make -j 4

# Run unit tests.
