#!/bin/sh

set -e
set -x

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
WEB_IMAGE=registry.centreon.com/mon-web-20.10:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfiles.
rm -rf centreon-build-containers
cp -r `dirname $0`/../../../containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < bam/20.10/Dockerfile.in > bam/Dockerfile
sed "s#@PROJECT@#$PROJECT#g;s#@SUBDIR@#20.10/el7/noarch/bam/$PROJECT-$VERSION-$RELEASE#g" < repo/centreon-internal.repo.in > repo/centreon-internal.repo

# Build image.
REGISTRY="registry.centreon.com"
BAM_IMAGE="$REGISTRY/des-bam-$VERSION-$RELEASE:$DISTRIB"
BAM_WIP_IMAGE="$REGISTRY/des-bam-20.10-wip:$DISTRIB"
docker build --no-cache -t "$BAM_IMAGE" -f bam/Dockerfile .
docker push "$BAM_IMAGE"
docker tag "$BAM_IMAGE" "$BAM_WIP_IMAGE"
docker push "$BAM_WIP_IMAGE"
