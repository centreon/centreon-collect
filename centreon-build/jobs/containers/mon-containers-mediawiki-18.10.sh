#!/bin/sh

set -e
set -x

# Pull base image.
docker pull mediawiki:latest

# Build image.
cd centreon-build/containers
docker build -t ci.int.centreon.com:5000/mon-mediawiki-18.10:latest -f mediawiki/18.10/mediawiki.Dockerfile .

# Push image.
docker push ci.int.centreon.com:5000/mon-mediawiki-18.10:latest
