#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
VERSION="$1"
case "$2" in
  centos7)
    BASE_IMAGE="centos:7"
    ;;
  centos8)
    BASE_IMAGE="registry.centreon.com/centos:8"
    ;;
  debian10)
    BASE_IMAGE="debian:buster"
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
cp dependencies/Dockerfile.in dependencies/Dockerfile
if [ -e "dependencies/$VERSION/Dockerfile.post.$DISTRIB.in" ] ; then
  cat "dependencies/$VERSION/Dockerfile.post.$DISTRIB.in" >> dependencies/Dockerfile
fi
sed -i -e "s#@VERSION@#$VERSION#g" -e "s#@DISTRIB@#$DISTRIB#g" -e "s#@BASE_IMAGE@#$BASE_IMAGE#g" dependencies/Dockerfile

# Build image.
DEP_IMG="registry.centreon.com/mon-dependencies-$VERSION:$DISTRIB"
docker pull "$BASE_IMAGE"
docker build --no-cache -t "$DEP_IMG" -f dependencies/Dockerfile .
docker push "$DEP_IMG"
