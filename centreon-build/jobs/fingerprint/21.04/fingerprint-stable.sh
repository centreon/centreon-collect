#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-fingerprint

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "standard" "21.04" "el7" "x86_64" "fingerprint" "fingerprint"
promote_testing_rpms_to_stable "standard" "21.04" "el8" "x86_64" "fingerprint" "fingerprint"