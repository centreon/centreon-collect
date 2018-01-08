#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
case "$1" in
  centos6)
    BASE_IMAGE="alanfranz/drb-epel-6-x86-64"
    ;;
  centos7)
    BASE_IMAGE="alanfranz/drb-epel-7-x86-64"
    ;;
  debian9)
    BASE_IMAGE="debian:9"
    ;;
  debian9-armhf)
    BASE_IMAGE="ci.int.centreon.com:5000/mon-build-dependencies:debian9"
    ;;
  opensuse423)
    BASE_IMAGE="opensuse:42.3"
    ;;
  *)
    echo "USAGE: $0 <distrib>"
    exit 1
    ;;
esac
DISTRIB="$1"

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed -e "s/@DISTRIB@/$DISTRIB/g" -e "s/@BASE_IMAGE@/$BASE_IMAGE/g" < build-dependencies/Dockerfile.in > build-dependencies/Dockerfile

# Build image.
BUILD_IMG="ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB"
docker pull "$BASE_IMAGE"
docker build --no-cache -t "$BUILD_IMG" -f build-dependencies/Dockerfile .
docker push "$BUILD_IMG"
