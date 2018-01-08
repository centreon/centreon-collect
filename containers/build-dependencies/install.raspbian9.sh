#!/bin/sh

set -e
set -x

# Install cross-compiler and armhf dependencies.
xargs apt-get install < /tmp/build-dependencies.txt
