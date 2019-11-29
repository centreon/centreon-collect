#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Base image.
BASE_IMAGE="registry.centreon.com/centos:7"
docker pull "$BASE_IMAGE"

# Prepare context.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
cp map/20.04/build-dependencies.Dockerfile.in Dockerfile
sed -i -e "s#@BASE_IMAGE@#$BASE_IMAGE#g" Dockerfile

# Build image.
TARGET_IMAGE="registry.centreon.com/map-build-dependencies-20.04:centos7"
docker build -t "$TARGET_IMAGE" .
docker push "$TARGET_IMAGE"
