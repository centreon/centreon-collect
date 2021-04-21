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

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  # Copy build artifacts.
  copy_internal_source_to_testing "standard" "awie" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "21.10" "el7" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "21.10" "el8" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE"

  # Docker image target version.
  TARGETVERSION="$VERSION"

#
# CI delivery.
#
else
  # Move RPMs to unstable.
  promote_canary_rpms_to_unstable "standard" "21.10" "el7" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE"
  promote_canary_rpms_to_unstable "standard" "21.10" "el8" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE"

  # Docker image target version.
  TARGETVERSION="21.10"
fi

# Tag Docker images.
REGISTRY='registry.centreon.com'
for distrib in centos7 centos8 ; do
  docker pull "$REGISTRY/mon-awie-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/mon-awie-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-awie-$TARGETVERSION:$distrib"
  docker push "$REGISTRY/mon-awie-$TARGETVERSION:$distrib"
done
