#!/bin/sh

set -e
set -x

# Install base tools.
apt-get update
apt-get install curl netcat-openbsd

# Install dependencies.
xargs apt-get install < /tmp/dependencies.txt
