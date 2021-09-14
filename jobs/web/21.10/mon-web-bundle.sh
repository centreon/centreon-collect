#!/bin/sh

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|centos8|...>"
  exit 1
fi
DISTRIB="$1"
case "$DISTRIB" in
  centos7)
    REPODISTRIB=el7
    ;;
  centos8)
    REPODISTRIB=el8
    ;;
  *)
    echo "Unsupported distribution $DISTRIB."
    exit 1
esac

# Target images.
REGISTRY="registry.centreon.com"
BASE_IMG="$REGISTRY/mon-dependencies-21.10:$DISTRIB"
FRESH_IMG="$REGISTRY/mon-web-fresh-$VERSION-$RELEASE:$DISTRIB"
FRESH_WIP_IMG=$(echo "$REGISTRY/mon-web-fresh-$BRANCH_NAME:$DISTRIB" | sed -e 's/\(.*\)/\L\1/')
STANDARD_IMG="$REGISTRY/mon-web-$VERSION-$RELEASE:$DISTRIB"
STANDARD_WIP_IMG=$(echo "$REGISTRY/mon-web-$BRANCH_NAME:$DISTRIB" | sed -e 's/\(.*\)/\L\1/')
WIDGETS_IMG="$REGISTRY/mon-web-widgets-$VERSION-$RELEASE:$DISTRIB"
WIDGETS_WIP_IMG=$(echo "$REGISTRY/mon-web-widgets-$BRANCH_NAME:$DISTRIB" | sed -e 's/\(.*\)/\L\1/')

# Pull base image.
docker pull "$BASE_IMG"

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s#@BASE_IMAGE@#$BASE_IMG#g" < "web/21.10/fresh.Dockerfile.$DISTRIB.in" > web/fresh.Dockerfile
sed "s#@BASE_IMAGE@#$FRESH_IMG#g" < web/21.10/standard.Dockerfile.in > web/standard.Dockerfile
sed "s#@BASE_IMAGE@#$STANDARD_IMG#g" < web/21.10/widgets.Dockerfile.in > web/widgets.Dockerfile

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
