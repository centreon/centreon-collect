#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
export PROJECT=centreon-autodiscovery

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "plugin-packs" "3.4" "el6" "x86_64" "autodisco" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "3.4" "el7" "x86_64" "autodisco" "$PROJECT-$VERSION-$RELEASE"
