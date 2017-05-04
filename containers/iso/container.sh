#!/bin/sh

set -e
set -x

cd /
/usr/local/bin/make-iso --mirror --installer --arch amd64 --version $1 --flavor standard
