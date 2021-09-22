#!/bin/sh

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-bam-server

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
WEB_IMAGE=registry.centreon.com/mon-web-21.10:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < bam/21.10/Dockerfile.in > bam/Dockerfile

# Build image.
REGISTRY="registry.centreon.com"
BAM_IMAGE="$REGISTRY/des-bam-$VERSION-$RELEASE:$DISTRIB"
BAM_WIP_IMAGE="$REGISTRY/des-bam-21.10-wip:$DISTRIB"
docker build --no-cache -t "$BAM_IMAGE" -f bam/Dockerfile .
docker push "$BAM_IMAGE"
docker tag "$BAM_IMAGE" "$BAM_WIP_IMAGE"
docker push "$BAM_WIP_IMAGE"

REGISTRY="registry.centreon.com"
if [ "$DISTRIB" = "centos7" -o "$DISTRIB" = "centos8" ] ; then
  docker pull "$REGISTRY/des-bam-$VERSION-$RELEASE:$DISTRIB"
  docker tag "$REGISTRY/des-bam-$VERSION-$RELEASE:$DISTRIB" "$REGISTRY/des-bam-21.10:$DISTRIB"
  docker push "$REGISTRY/des-bam-21.10:$DISTRIB"
fi