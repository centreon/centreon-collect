#!/bin/sh

set -e
set -x

# Pull base image.
docker pull centos:6

# Build image.
cd centreon-build/containers
docker build -t ci.int.centreon.com:5000/mon-mediawiki:latest -f mediawiki/mediawiki.Dockerfile .

# Push image.
docker push ci.int.centreon.com:5000/mon-mediawiki:latest
