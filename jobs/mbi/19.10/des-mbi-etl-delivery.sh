#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-bi-etl

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "mbi" "mbi-etl" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "mbi" "19.10" "el7" "noarch" "mbi-etl" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "mbi" "19.10" "el7" "noarch" "mbi-etl" "$PROJECT-$VERSION-$RELEASE"
fi
