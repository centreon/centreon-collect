#!/bin/sh

set -e
set -x

# Pull base image.
docker pull ubuntu:latest

# Build image.
cd `dirname $0`/../../containers
docker build -t ci.int.centreon.com:5000/mon-build-iso-3.4-4:latest -f iso/3.4-4/build-iso.Dockerfile .

# Push image.
docker push ci.int.centreon.com:5000/mon-build-iso-3.4-4:latest
