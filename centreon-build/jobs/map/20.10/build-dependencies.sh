#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Base image.
BASE_IMAGE="maven:3-jdk-11"
docker pull "$BASE_IMAGE"

# Prepare context.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
cp map/20.10/build-dependencies.Dockerfile.in Dockerfile
sed -i -e "s#@BASE_IMAGE@#$BASE_IMAGE#g" Dockerfile

# Build image.
TARGET_IMAGE_CENTOS7="registry.centreon.com/map-build-dependencies-20.10:centos7"
TARGET_IMAGE_CENTOS8="registry.centreon.com/map-build-dependencies-20.10:centos8"
docker build -t "$TARGET_IMAGE_CENTOS7" .
docker build -t "$TARGET_IMAGE_CENTOS8" .
docker push "$TARGET_IMAGE_CENTOS7"
docker push "$TARGET_IMAGE_CENTOS8"
