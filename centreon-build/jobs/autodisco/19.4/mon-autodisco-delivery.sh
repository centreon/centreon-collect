#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-autodiscovery

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='ci.int.centreon.com:5000'
for distrib in centos7 ; do
  docker pull "$REGISTRY/mon-autodisco-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/mon-autodisco-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-autodisco-19.4:$distrib"
  docker push "$REGISTRY/mon-autodisco-19.4:$distrib"
done

# Move RPMs to unstable.
promote_canary_rpms_to_unstable "standard" "19.4" "el7" "x86_64" "autodisco" "$PROJECT-$VERSION-$RELEASE"
