#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
export PROJECT=centreon-ha

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/standard/testing/ha/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Put sources online.
upload_tarball_for_download "$PROJECT" "$VERSION" "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz" "s3://centreon-download/public/$PROJECT/$PROJECT-$VERSION.tar.gz"

# Move RPMs to the stable repository.
promote_rpms_from_testing_to_stable "standard" "21.04" "el7" "noarch" "ha" "$PROJECT-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "21.04" "el8" "noarch" "ha" "$PROJECT-$VERSION-$RELEASE"
