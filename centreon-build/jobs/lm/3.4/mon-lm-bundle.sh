#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-license-manager

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEB_IMAGE=ci.int.centreon.com:5000/mon-web-2.8:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < lm/3.4/lm.Dockerfile.in > lm/lm.$DISTRIB.Dockerfile

# Build image.
REGISTRY="ci.int.centreon.com:5000"
LM_IMAGE="$REGISTRY/mon-lm-$VERSION-$RELEASE:$DISTRIB"
docker build --no-cache -t "$LM_IMAGE" -f lm/lm.$DISTRIB.Dockerfile .
docker push "$LM_IMAGE"
