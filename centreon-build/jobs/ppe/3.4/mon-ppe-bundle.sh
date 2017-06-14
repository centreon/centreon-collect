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

# Pull Centreon Web image.
WEB_IMAGE=ci.int.centreon.com:5000/mon-web-3.4:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < ppe/3.4/ppe.Dockerfile.in > ppe/Dockerfile

# Build image.
REGISTRY="ci.int.centreon.com:5000"
PPE_IMAGE="$REGISTRY/mon-ppe-$VERSION-$RELEASE:$DISTRIB"
PPE_WIP_IMAGE="$REGISTRY/mon-ppe-3.4-wip:$DISTRIB"
docker build --no-cache -t "$PPE_IMAGE" -f ppe/Dockerfile .
docker push "$PPE_IMAGE"
docker tag "$PPE_IMAGE" "$PPE_WIP_IMAGE"
docker push "$PPE_WIP_IMAGE"
