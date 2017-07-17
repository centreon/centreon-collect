#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
case "$1" in
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
DISTRIB="$1"

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" "s/@BASE_IMAGE@/$BASE_IMAGE/g" < dependencies/Dockerfile.in > dependencies/Dockerfile

# Build image.
DEP_IMG="ci.int.centreon.com:5000/mon-dependencies:$DISTRIB"
docker build --no-cache -t "$DEP_IMG" -f dependencies/Dockerfile .
docker push "$DEP_IMG"
