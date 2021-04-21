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

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "standard" "lm" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "21.10" "el7" "noarch" "lm" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "21.10" "el8" "noarch" "lm" "$PROJECT-$VERSION-$RELEASE"
  TARGETVERSION="$VERSION"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "standard" "21.10" "el7" "noarch" "lm" "$PROJECT-$VERSION-$RELEASE"
  promote_canary_rpms_to_unstable "standard" "21.10" "el8" "noarch" "lm" "$PROJECT-$VERSION-$RELEASE"
  TARGETVERSION="21.10"
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for distrib in centos7 centos8 ; do
  docker pull "$REGISTRY/mon-lm-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/mon-lm-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-lm-$TARGETVERSION:$distrib"
  docker push "$REGISTRY/mon-lm-$TARGETVERSION:$distrib"
done
