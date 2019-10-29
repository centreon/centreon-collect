#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
VERSION="$1"
case "$2" in
  centos6)
    BASE_IMAGE="alanfranz/drb-epel-6-x86-64"
    ;;
  centos7)
    BASE_IMAGE="alanfranz/drb-epel-7-x86-64"
    ;;
  debian10)
    BASE_IMAGE="debian:buster"
    ;;
  debian10-armhf)
    BASE_IMAGE="registry.centreon.com/mon-build-dependencies-$VERSION:debian10"
    ;;
  opensuse-leap)
    BASE_IMAGE="opensuse/leap:latest"
    ;;
  *)
    echo "USAGE: $0 <distrib>"
    exit 1
    ;;
esac
DISTRIB="$2"

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
cp build-dependencies/Dockerfile.in build-dependencies/Dockerfile
if [ -e "build-dependencies/$VERSION/Dockerfile.post.$DISTRIB.in" ] ; then
  cat "build-dependencies/$VERSION/Dockerfile.post.$DISTRIB.in" >> build-dependencies/Dockerfile
fi
sed -i -e "s#@VERSION@#$VERSION#g" -e "s#@DISTRIB@#$DISTRIB#g" -e "s#@BASE_IMAGE@#$BASE_IMAGE#g" build-dependencies/Dockerfile

# Build image.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-$VERSION:$DISTRIB"
docker pull "$BASE_IMAGE"
docker build --no-cache -t "$BUILD_IMG" -f build-dependencies/Dockerfile .
docker push "$BUILD_IMG"
