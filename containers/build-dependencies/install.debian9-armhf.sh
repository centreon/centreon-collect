#!/bin/sh

set -e
set -x

# Install cross-compiler and armhf dependencies.
apt-get update
xargs apt-get install < /tmp/build-dependencies.txt
