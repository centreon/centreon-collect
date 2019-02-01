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

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for distrib in centos7 ; do
  docker pull "$REGISTRY/des-bam-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/des-bam-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-bam-19.04:$distrib"
  docker push "$REGISTRY/des-bam-19.04:$distrib"
done

# Move RPMs to unstable.
promote_canary_rpms_to_unstable "bam" "19.04" "el7" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE"
