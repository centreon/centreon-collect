#!/bin/sh

set -e
set -x

# Pull base image.
docker pull osixia/openldap:latest

# Build image.
cd centreon-build/containers
docker build -t registry.centreon.com/mon-openldap:latest -f openldap/openldap.Dockerfile .

# Push image
docker push registry.centreon.com/mon-openldap:latest
