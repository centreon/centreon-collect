#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=perl-Statistics-Regression

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "standard" "20.10" "el7" "noarch" "$PROJECT" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.10" "el8" "noarch" "$PROJECT" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.04" "el7" "noarch" "$PROJECT" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.04" "el8" "noarch" "$PROJECT" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.10" "el7" "noarch" "$PROJECT" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.10" "el8" "noarch" "$PROJECT" "$PROJECT-$VERSION-$RELEASE"
