#!/bin/sh

set -e
set -x

# Pull base image.
docker pull debian:jessie

# Build image.
cd centreon-build/containers
docker build -t registry.centreon.com/mon-lemonldap:latest -f lemonldap/lemonldap.Dockerfile .

# Push image
docker push registry.centreon.com/mon-lemonldap:latest
