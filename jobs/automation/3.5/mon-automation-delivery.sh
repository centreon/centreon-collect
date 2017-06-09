#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='ci.int.centreon.com:5000'
AUTOMATION_CENTOS6="$REGISTRY/mon-automation-$VERSION-$RELEASE:centos6"
AUTOMATION_CENTOS7="$REGISTRY/mon-automation-$VERSION-$RELEASE:centos7"

docker pull "$AUTOMATION_CENTOS6"
docker tag "$AUTOMATION_CENTOS6" "$REGISTRY/mon-automation:centos6"
docker push "$REGISTRY/mon-automation:centos6"

docker pull "$AUTOMATION_CENTOS7"
docker tag "$AUTOMATION_CENTOS7" "$REGISTRY/mon-automation:centos7"
docker push "$REGISTRY/mon-automation:centos7"
