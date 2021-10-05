#!/bin/sh

set -e

. `dirname $0`/../../common.sh

# Project.
export PROJECT=centreon-connector

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/standard/testing/connector/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Put sources online.
upload_tarball_for_download centreon-connectors "$VERSION" "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/centreon-connectors-$VERSION.tar.gz" "s3://centreon-download/public/centreon-connectors/centreon-connectors-$VERSION.tar.gz"

# Move RPMs to the stable repository.
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el7" "x86_64" "connector" "$PROJECT-$VERSION-$RELEASE"