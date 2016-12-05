#!/bin/sh

set -e
set -x

# Pull base image.
docker pull centos:7

# Build image.
cd centreon-build/containers
docker build -t ci.int.centreon.com:5000/mon-squid:latest -f squid/squid.Dockerfile .

# Push image.
docker push ci.int.centreon.com:5000/mon-squid:latest
