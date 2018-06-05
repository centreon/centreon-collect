#!/bin/sh

set -e
set -x

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

# Pull base image.
WEB_IMAGE=ci.int.centreon.com:5000/mon-web-3.5:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < bam/3.5/Dockerfile.in > bam/Dockerfile

# Build image.
REGISTRY="ci.int.centreon.com:5000"
BAM_IMAGE="$REGISTRY/des-bam-$VERSION-$RELEASE:$DISTRIB"
BAM_WIP_IMAGE="$REGISTRY/des-bam-3.5-wip:$DISTRIB"
docker build --no-cache -t "$BAM_IMAGE" -f bam/Dockerfile .
docker push "$BAM_IMAGE"
docker tag "$BAM_IMAGE" "$BAM_WIP_IMAGE"
docker push "$BAM_WIP_IMAGE"
