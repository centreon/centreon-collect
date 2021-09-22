#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-ha

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
  copy_internal_source_to_testing "standard" "ha" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "20.10" "el7" "noarch" "ha" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "20.10" "el8" "noarch" "ha" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "standard" "20.10" "el7" "noarch" "ha" "$PROJECT-$VERSION-$RELEASE"
  promote_canary_rpms_to_unstable "standard" "20.10" "el8" "noarch" "ha" "$PROJECT-$VERSION-$RELEASE"
fi
