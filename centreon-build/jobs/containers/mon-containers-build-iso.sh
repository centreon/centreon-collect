#!/bin/sh

set -e
set -x

# Pull base image.
docker pull centos:7.4.1708

# Build image.
cd `dirname $0`/../../containers
docker build -t ci.int.centreon.com:5000/mon-build-iso:centos7 -f iso/centos7/build-iso.Dockerfile .

# Push image.
docker push ci.int.centreon.com:5000/mon-build-iso:centos7
