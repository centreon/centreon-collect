#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-agent

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  put_testing_source "standard" "agent" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
  copy_internal_rpms_to_testing "standard" "20.04" "el7" "x86_64" "agent" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "standard" "20.04" "el7" "x86_64" "agent" "$PROJECT-$VERSION-$RELEASE"
fi
