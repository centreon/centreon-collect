#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-license-manager

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|...>"
  exit 1
fi
DISTRIB="$1"

# Pull images.
WEB_IMAGE=registry.centreon.com/mon-web-21.10:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < lm/21.10/lm.Dockerfile.in > lm/lm.$DISTRIB.Dockerfile
if [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIBCODENAME=el7
elif [ "$DISTRIB" = 'centos8' ] ; then
  DISTRIBCODENAME=el8
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
sed "s#@PROJECT@#$PROJECT#g;s#@SUBDIR@#21.10/$DISTRIBCODENAME/noarch/lm/$PROJECT-$VERSION-$RELEASE#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo

# Build image.
REGISTRY="registry.centreon.com"
LM_IMAGE="$REGISTRY/mon-lm-$VERSION-$RELEASE:$DISTRIB"
LM_WIP_IMAGE="$REGISTRY/mon-lm-21.10-wip:$DISTRIB"
docker build --no-cache -t "$LM_IMAGE" -f lm/lm.$DISTRIB.Dockerfile .
docker push "$LM_IMAGE"
docker tag "$LM_IMAGE" "$LM_WIP_IMAGE"
docker push "$LM_WIP_IMAGE"
