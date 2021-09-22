#!/bin/sh

set -e
set -x

# Pull base image.
docker pull mediawiki:latest

# Build image.
cd centreon-build/containers
docker build -t registry.centreon.com/mon-mediawiki-19.10:latest -f mediawiki/19.10/mediawiki.Dockerfile .

# Push image.
docker push registry.centreon.com/mon-mediawiki-19.10:latest
