#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-web

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Put sources online.
SRCHASH=$SSH_REPO md5sum "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz" "s3://centreon-download/public/centreon/$PROJECT-$VERSION.tar.gz"
curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=1"

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh

# Generate online documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon -V latest -p'"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon -V latest -p'"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon -V 2.8.x -p'"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon -V 2.8.x -p'"
