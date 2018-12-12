#!/bin/sh

set -e
set -x

# Pull base image.
docker pull centos:7

# Build image.
cd centreon-build/containers
docker build -t registry.centreon.com/mon-phantomjs:latest -f webdrivers/phantomjs.Dockerfile .

# Push image.
docker push registry.centreon.com/mon-phantomjs:latest
