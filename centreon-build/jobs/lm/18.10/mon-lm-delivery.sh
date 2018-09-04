#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-license-manager

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='ci.int.centreon.com:5000'
for distrib in centos7 ; do
  docker pull "$REGISTRY/mon-lm-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/mon-lm-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-lm-18.10:$distrib"
  docker push "$REGISTRY/mon-lm-18.10:$distrib"
done

# Move RPMs to unstable.
move_internal_rpms_to_unstable "standard" "18.10" "el7" "noarch" "lm" "$PROJECT-$VERSION-$RELEASE"
