#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-export

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

# Pull Centreon Web image.
WEB_IMAGE=registry.centreon.com/mon-ppm-autodisco-19.10:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < ppe/19.10/ppe.Dockerfile.in > ppe/Dockerfile
sed "s#@PROJECT@#$PROJECT#g;s#@SUBDIR@#19.10/el7/noarch/ppe/$PROJECT-$VERSION-$RELEASE#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo

# Build image.
REGISTRY="registry.centreon.com"
PPE_IMAGE="$REGISTRY/mon-ppe-$VERSION-$RELEASE:$DISTRIB"
PPE_WIP_IMAGE="$REGISTRY/mon-ppe-19.10-wip:$DISTRIB"
docker build --no-cache -t "$PPE_IMAGE" -f ppe/Dockerfile .
docker push "$PPE_IMAGE"
docker tag "$PPE_IMAGE" "$PPE_WIP_IMAGE"
docker push "$PPE_WIP_IMAGE"
