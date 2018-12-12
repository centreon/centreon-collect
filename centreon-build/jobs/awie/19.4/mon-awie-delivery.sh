#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-awie

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for distrib in centos7 ; do
  docker pull "$REGISTRY/mon-awie-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/mon-awie-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-awie-19.4:$distrib"
  docker push "$REGISTRY/mon-awie-19.4:$distrib"
done

# Move RPMs to unstable.
promote_canary_rpms_to_unstable "standard" "19.4" "el7" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE"
