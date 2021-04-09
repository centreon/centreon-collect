#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-AS400

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
# 20.04
promote_testing_rpms_to_stable "standard" "20.04" "el7" "x86_64" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.04" "el8" "x86_64" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.04" "el7" "noarch" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.04" "el8" "noarch" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"

# 20.10
promote_testing_rpms_to_stable "standard" "20.10" "el7" "x86_64" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.10" "el8" "x86_64" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.10" "el7" "noarch" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.10" "el8" "noarch" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"

# 21.04
promote_testing_rpms_to_stable "standard" "21.04" "el7" "x86_64" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.04" "el8" "x86_64" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.04" "el7" "noarch" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.04" "el8" "noarch" "centreon-as400" "$PROJECT-$VERSION-$RELEASE"