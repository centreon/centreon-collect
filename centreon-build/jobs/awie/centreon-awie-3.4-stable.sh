#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-awie

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/lts/testing/$PROJECT-$VERSION-$RELEASE" "/srv/sources/lts/stable/"

# Put sources online.
upload_artifact_for_download "$PROJECT" 3.4 "$VERSION" tar.gz 0 "/srv/sources/lts/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz" "s3://centreon-download/public/$PROJECT/$PROJECT-$VERSION.tar.gz"

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh
`dirname $0`/../sync-repo.sh --project lts --path /3.4 --confirm

# Generate online documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-awie -V latest -p'"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-awie -V latest -p'"
