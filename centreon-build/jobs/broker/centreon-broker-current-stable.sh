#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-broker

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Put sources online.
SRCHASH=`$SSH_REPO "md5sum /srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz | cut -d ' ' -f 1"`
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz" "s3://centreon-download/public/$PROJECT/$PROJECT-$VERSION.tar.gz"
curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=0"

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh
$SSH_REPO /srv/scripts/sync-standard.sh --confirm

# Generate online documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-broker -V latest -p'"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-broker -V 3.0 -p'"
