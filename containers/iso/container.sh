#!/bin/sh

set -e
set -x

cd /tmp
/usr/local/bin/make-iso --mirror --installer --arch amd64 --version $1 --flavor standard
