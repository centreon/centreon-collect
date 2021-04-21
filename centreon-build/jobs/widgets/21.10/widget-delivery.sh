#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$WIDGET" ] ; then
  echo "You need to specify VERSION, RELEASE and WIDGET environment variables."
  exit 1
fi

# Project.
PROJECT=centreon-widget-$WIDGET

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "standard" "widget-$WIDGET" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "21.10" "el7" "noarch" "widget-$WIDGET" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "21.10" "el8" "noarch" "widget-$WIDGET" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  # Move RPMs to unstable.
  promote_canary_rpms_to_unstable "standard" "21.10" "el7" "noarch" "widget-$WIDGET" "$PROJECT-$VERSION-$RELEASE"
  promote_canary_rpms_to_unstable "standard" "21.10" "el8" "noarch" "widget-$WIDGET" "$PROJECT-$VERSION-$RELEASE"
fi
