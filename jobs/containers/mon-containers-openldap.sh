#!/bin/sh

set -e
set -x

# Pull base image.
docker pull osixia/openldap:latest

# Build image.
cd centreon-build/containers
docker build -t ci.int.centreon.com:5000/mon-openldap:latest -f openldap/Dockerfile .

# Push image
docker push ci.int.centreon.com:5000/mon-openldap:latest
