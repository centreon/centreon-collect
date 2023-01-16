#!/bin/bash
set -e 
set -x

source common.sh

# Check arguments
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION / RELEASE variables"
  exit 1
fi

MAJOR=`echo $VERSION | cut -d . -f 1,2`

# Move sources to the stable directory.
ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com cp -r "/srv/sources/standard/testing/centreon-collect/centreon-collect-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Put sources online.
upload_tarball_for_download "centreon-collect" "$VERSION" "/srv/sources/standard/stable/centreon-collect-$VERSION-$RELEASE/centreon-collect-$VERSION-$RELEASE.tar.gz" "s3://centreon-download/public/centreon-collect/centreon-collect-$VERSION.tar.gz" "$1"

# Move RPMs to the stable repository.
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el7" "x86_64" "broker" "centreon-broker-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el8" "x86_64" "broker" "centreon-broker-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el7" "x86_64" "engine" "centreon-engine-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el8" "x86_64" "engine" "centreon-engine-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el7" "x86_64" "clib" "centreon-clib-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el8" "x86_64" "clib" "centreon-clib-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el7" "x86_64" "connector" "centreon-connector-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el8" "x86_64" "connector" "centreon-connector-$VERSION-$RELEASE"
