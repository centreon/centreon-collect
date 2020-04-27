#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-vmware

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the testing repository.
promote_unstable_rpms_to_testing "standard" "3.4" "el6" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "3.4" "el7" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "19.04" "el7" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "19.10" "el7" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
