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
$SSH_REPO mv "/srv/sources/standard/testing/widget/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Put sources online.
SRCHASH=`$SSH_REPO "md5sum /srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz | cut -d ' ' -f 1"`
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz" "s3://centreon-download/public/centreon-widgets/$PROJECT/$PROJECT-$VERSION.tar.gz"
curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=0"

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "standard" "18.10" "el7" "noarch" "widget" "$PROJECT-$VERSION-$RELEASE"
