#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Check arguments.
if [ -z "$WIDGET" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify WIDGET, VERSION and RELEASE environment variables."
  exit 1
fi

# Project.
export PROJECT="centreon-widget-$WIDGET"

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Put sources online.
upload_artifact_for_download "$PROJECT" 3.4 "$VERSION" tar.gz 0 "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz" "s3://centreon-download/public/centreon-widgets/$PROJECT/$PROJECT-$VERSION.tar.gz"

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh
$SSH_REPO /srv/scripts/sync-standard.sh --confirm
