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
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz" "s3://centreon-download/public/centreon-broker/$PROJECT-$VERSION.tar.gz"

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh

# Generate online documentation.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-broker -V latest -p'"
