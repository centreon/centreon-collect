#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <widget_name>"
  exit 1
fi
WIDGET="$1"

# Project.
PROJECT=centreon-widget-$WIDGET

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "standard" "widgets" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "20.10" "el7" "noarch" "widgets" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  # Move RPMs to unstable.
  promote_canary_rpms_to_unstable "standard" "20.10" "el7" "noarch" "widgets" "$PROJECT-$VERSION-$RELEASE"
fi
