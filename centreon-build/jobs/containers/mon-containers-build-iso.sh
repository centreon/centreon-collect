#!/bin/sh

set -e
set -x

# Pull base image.
docker pull ubuntu:latest

# Build image.
cd centreon-build/containers
docker build -t ci.int.centreon.com:5000/mon-build-iso:latest -f build-iso.Dockerfile .

# Push image.
docker push ci.int.centreon.com:5000/mon-build-iso:latest
