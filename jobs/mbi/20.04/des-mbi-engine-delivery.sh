#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-bi-engine

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "mbi" "mbi-engine" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "mbi" "20.04" "el7" "noarch" "mbi-engine" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "mbi" "20.04" "el7" "noarch" "mbi-engine" "$PROJECT-$VERSION-$RELEASE"
fi
