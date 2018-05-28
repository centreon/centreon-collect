#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
VERSION="$1"
case "$2" in
  centos6)
    BASE_IMAGE="centos:6"
    ;;
  centos7)
    BASE_IMAGE="centos:7"
    ;;
  debian9)
    BASE_IMAGE="debian:9"
    ;;
  *)
    echo "USAGE: $0 <distrib>"
    exit 1
    ;;
esac
DISTRIB="$2"
docker pull "$BASE_IMAGE"

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed -e "@VERSION@/$VERSION/g" -e "s/@DISTRIB@/$DISTRIB/g" -e "s/@BASE_IMAGE@/$BASE_IMAGE/g" < dependencies/Dockerfile.in > dependencies/Dockerfile

# Build image.
DEP_IMG="ci.int.centreon.com:5000/mon-dependencies-$VERSION:$DISTRIB"
docker build --no-cache -t "$DEP_IMG" -f dependencies/Dockerfile .
docker push "$DEP_IMG"
