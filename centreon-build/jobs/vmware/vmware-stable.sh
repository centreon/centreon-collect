#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
export PROJECT=centreon-vmware

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "standard" "3.4" "el6" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "3.4" "el7" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "18.10" "el7" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "19.04" "el7" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "19.10" "el7" "noarch" "vmware" "$PROJECT-$VERSION-$RELEASE"
