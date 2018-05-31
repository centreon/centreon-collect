#!/bin/sh

set -e
set -x

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
if [ "$DISTRIB" = "centos6" ] ; then
  CENTOS_VERSION=6
else
  CENTOS_VERSION=7
fi

# Target images.
REGISTRY="ci.int.centreon.com:5000"
BASE_IMG="$REGISTRY/mon-dependencies-3.4:$DISTRIB"
FRESH_IMG="$REGISTRY/mon-web-fresh-$VERSION-$RELEASE:$DISTRIB"
FRESH_WIP_IMG="$REGISTRY/mon-web-fresh-3.4-wip:$DISTRIB"
STANDARD_IMG="$REGISTRY/mon-web-$VERSION-$RELEASE:$DISTRIB"
STANDARD_WIP_IMG="$REGISTRY/mon-web-3.4-wip:$DISTRIB"
WIDGETS_IMG="$REGISTRY/mon-web-widgets-$VERSION-$RELEASE:$DISTRIB"
WIDGETS_WIP_IMG="$REGISTRY/mon-web-widgets-3.4-wip:$DISTRIB"

# Pull base image.
docker pull "$BASE_IMG"

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s#@BASE_IMAGE@#$BASE_IMG#g" < web/3.4/fresh.Dockerfile.in > web/fresh.Dockerfile
sed "s#@BASE_IMAGE@#$FRESH_IMG#g" < web/3.4/standard.Dockerfile.in > web/standard.Dockerfile
sed "s#@BASE_IMAGE@#$STANDARD_IMG#g" < web/3.4/widgets.Dockerfile.in > web/widgets.Dockerfile
sed "s#@VERSION@#3.4#g;s#@DISTRIB@#el$CENTOS_VERSION#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo

# Build 'fresh' image.
docker build --no-cache --ulimit 'nofile=40000' -t "$FRESH_IMG" -f web/fresh.Dockerfile .
docker push "$FRESH_IMG"
docker tag "$FRESH_IMG" "$FRESH_WIP_IMG"
docker push "$FRESH_WIP_IMG"

# Build 'standard' image.
docker build --no-cache -t "$STANDARD_IMG" -f web/standard.Dockerfile .
docker push "$STANDARD_IMG"
docker tag "$STANDARD_IMG" "$STANDARD_WIP_IMG"
docker push "$STANDARD_WIP_IMG"

# Build 'widgets' image.
docker build --no-cache -t "$WIDGETS_IMG" -f web/widgets.Dockerfile .
docker push "$WIDGETS_IMG"
docker tag "$WIDGETS_IMG" "$WIDGETS_WIP_IMG"
docker push "$WIDGETS_WIP_IMG"
