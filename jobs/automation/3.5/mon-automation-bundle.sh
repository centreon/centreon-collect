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

# Pull images.
WEB_IMAGE=ci.int.centreon.com:5000/mon-ppm-3.5:$DISTRIB
docker pull $WEB_IMAGE

# Prepare Dockerfile.
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers
sed "s/@DISTRIB@/$DISTRIB/g" < automation/3.5/automation.Dockerfile.in > automation/automation.Dockerfile

# Build image.
REGISTRY="ci.int.centreon.com:5000"
AUTOMATION_IMAGE="$REGISTRY/mon-automation-$VERSION-$RELEASE:$DISTRIB"
AUTOMATION_WIP_IMAGE="$REGISTRY/mon-automation-3.5-wip:$DISTRIB"
docker build --no-cache -t "$AUTOMATION_IMAGE" -f automation/automation.Dockerfile .
docker push "$AUTOMATION_IMAGE"
docker tag "$AUTOMATION_IMAGE" "$AUTOMATION_WIP_IMAGE"
docker push "$AUTOMATION_WIP_IMAGE"
