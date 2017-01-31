#!/bin/sh

set -e
set -x

# Pull base image.
docker pull ci.int.centreon.com:5000/mon-squid-simple:latest

# Build image.
cd centreon-build/containers
docker build -t ci.int.centreon.com:5000/mon-squid-basic-auth:latest -f squid/basic-auth/squid.Dockerfile .

# Push image.
docker push ci.int.centreon.com:5000/mon-squid-basic-auth:latest
