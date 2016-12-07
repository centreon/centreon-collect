#!/bin/sh

set -e
set -x

# Pull base image.
docker pull debian:jessie

# Build image.
cd centreon-build/containers
docker build -t ci.int.centreon.com:5000/mon-lemonldap:latest -f lemonldap/lemonldap.Dockerfile .

# Push image
docker push ci.int.centreon.com:5000/mon-lemonldap:latest
