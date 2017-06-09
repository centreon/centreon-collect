#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-poller-display

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

# Target images.
REGISTRY="ci.int.centreon.com:5000"
WEB_FRESH_IMG="$REGISTRY/mon-web-fresh-3.4:$DISTRIB"
WEB_STANDARD_IMG="$REGISTRY/mon-web-3.4:$DISTRIB"
CENTRAL_IMG="$REGISTRY/mon-poller-display-central-$VERSION-$RELEASE:$DISTRIB"
POLLER_IMG="$REGISTRY/mon-poller-display-$VERSION-$RELEASE:$DISTRIB"

# Pull base images.
docker pull "$WEB_FRESH_IMG"
docker pull "$WEB_STANDARD_IMG"

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s#@BASE_IMAGE@#$WEB_FRESH_IMG#g" < poller-display/3.4/poller.Dockerfile.in > poller-display/poller.Dockerfile
sed "s#@BASE_IMAGE@#$WEB_STANDARD_IMG#g" < poller-display/3.4/central.Dockerfile.in > poller-display/central.Dockerfile

# Build 'poller' image.
docker build --no-cache -t "$POLLER_IMG" -f poller-display/poller.Dockerfile .
docker push "$POLLER_IMG"

# Build 'central' image.
docker build --no-cache -t "$CENTRAL_IMG" -f poller-display/central.Dockerfile .
docker push "$CENTRAL_IMG"
