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
POLLER_CENTOS6="$REGISTRY/mon-poller-display-$VERSION-$RELEASE:centos6"
POLLER_CENTOS7="$REGISTRY/mon-poller-display-$VERSION-$RELEASE:centos7"
CENTRAL_CENTOS6="$REGISTRY/mon-poller-display-central-$VERSION-$RELEASE:centos6"
CENTRAL_CENTOS7="$REGISTRY/mon-poller-display-central-$VERSION-$RELEASE:centos7"

# docker pull "$POLLER_CENTOS6"
# docker tag "$POLLER_CENTOS6" "$REGISTRY/mon-poller-display:centos6"
# docker push "$REGISTRY/mon-poller-display:centos6"

# docker pull "$POLLER_CENTOS7"
# docker tag "$POLLER_CENTOS7" "$REGISTRY/mon-poller-display:centos7"
# docker push "$REGISTRY/mon-poller-display:centos7"

docker pull "$CENTRAL_CENTOS6"
docker tag "$CENTRAL_CENTOS6" "$REGISTRY/mon-poller-display-central:centos6"
docker push "$REGISTRY/mon-poller-display-central:centos6"

docker pull "$CENTRAL_CENTOS7"
docker tag "$CENTRAL_CENTOS7" "$REGISTRY/mon-poller-display-central:centos7"
docker push "$REGISTRY/mon-poller-display-central:centos7"
