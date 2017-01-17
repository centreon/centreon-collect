#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Pull mon-dependencies image.
docker pull ci.int.centreon.com:5000/mon-dependencies:centos$CENTOS_VERSION

# Prepare Dockerfiles.
rm -rf centreon-build
cp -r /opt/centreon-build .
cd centreon-build/containers
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < web/fresh.Dockerfile.in > web/fresh.centos$CENTOS_VERSION.Dockerfile
sed "s/@CENTOS_VERSION@/$CENTOS_VERSION/g" < web/standard.Dockerfile.in > web/standard.centos$CENTOS_VERSION.Dockerfile

# Build 'fresh' image.
FRESH_IMG="ci.int.centreon.com:5000/mon-web-fresh-$VERSION-$RELEASE:centos$CENTOS_VERSION"
docker build --no-cache --ulimit 'nofile=40000' -t "$FRESH_IMG" -f web/fresh.centos$CENTOS_VERSION.Dockerfile .
docker push "$FRESH_IMG"

# Build 'standard' image.
STANDARD_IMG="ci.int.centreon.com:5000/mon-web-$VERSION-$RELEASE:centos$CENTOS_VERSION"
docker build --no-cache -t "$STANDARD_IMG" -f web/standard.centos$CENTOS_VERSION.Dockerfile .
docker push "$STANDARD_IMG"
