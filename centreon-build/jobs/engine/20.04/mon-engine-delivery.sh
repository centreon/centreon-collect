#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-engine

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "standard" "engine" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "20.04" "el7" "x86_64" "engine" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "standard" "20.04" "el7" "x86_64" "engine" "$PROJECT-$VERSION-$RELEASE"
fi
