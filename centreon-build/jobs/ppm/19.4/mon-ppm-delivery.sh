#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-pp-manager

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for distrib in centos7 ; do
  docker pull "$REGISTRY/mon-ppm-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/mon-ppm-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-ppm-19.4:$distrib"
  docker push "$REGISTRY/mon-ppm-19.4:$distrib"
done

# Move RPMs to unstable.
promote_canary_rpms_to_unstable "standard" "19.4" "el7" "noarch" "ppm" "$PROJECT-$VERSION-$RELEASE"
