#!/bin/sh

set -e
set -x

# Pull base image.
docker pull registry.centreon.com/mon-squid-simple:latest

# Build image.
cd centreon-build/containers
docker build -t registry.centreon.com/mon-squid-basic-auth:latest -f squid/basic-auth/squid.Dockerfile .

# Push image.
docker push registry.centreon.com/mon-squid-basic-auth:latest
