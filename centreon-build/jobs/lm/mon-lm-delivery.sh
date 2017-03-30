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
LM_CENTOS6="$REGISTRY/mon-lm-$VERSION-$RELEASE:centos6"
LM_CENTOS7="$REGISTRY/mon-lm-$VERSION-$RELEASE:centos7"

docker pull "$LM_CENTOS6"
docker tag "$LM_CENTOS6" "$REGISTRY/mon-lm:centos6"
docker push "$REGISTRY/mon-lm:centos6"

docker pull "$LM_CENTOS7"
docker tag "$LM_CENTOS7" "$REGISTRY/mon-lm:centos7"
docker push "$REGISTRY/mon-lm:centos7"
