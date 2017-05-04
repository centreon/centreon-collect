#!/bin/sh

set -e
set -x

/usr/local/bin/make-iso --mirror --installer --arch amd64 --version $1 --flavor standard
mv centreon-standard-$1-x86_64.iso centreon-$1.$2-el6-x86_64.iso
