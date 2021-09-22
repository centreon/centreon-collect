#!/bin/sh

set -e
set -x

# Pull base image.
docker pull centos:7

# Build image.
cd centreon-build/containers
docker build -t registry.centreon.com/mon-mediawiki-3.4:latest -f mediawiki/3.4/mediawiki.Dockerfile .

# Push image.
docker push registry.centreon.com/mon-mediawiki-3.4:latest
