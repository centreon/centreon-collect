#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
export PROJECT=centreon-anomaly-detection

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the stable repository.
promote_rpms_from_testing_to_stable "standard" "21.04" "el7" "noarch" "anomaly-detection" "$PROJECT-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "21.04" "el8" "noarch" "anomaly-detection" "$PROJECT-$VERSION-$RELEASE"
