#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$PROJECT" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify PROJECT, VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
if [ "$PROJECT" = "centreon-bi-server" ] ; then
  # Move sources to the stable directory.
  $SSH_REPO mv "/srv/sources/mbi/testing/$PROJECT-$VERSION-$RELEASE" "/srv/sources/mbi/stable/"

  # Put sources online.
  SRCHASH=`$SSH_REPO "cat /srv/sources/mbi/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-php{53,54}.tar.gz | md5sum | cut -d ' ' -f 1"`
  for phpversion in 53 54 ; do
    $SSH_REPO aws s3 cp --acl public-read "/srv/sources/mbi/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-php$phpversion.tar.gz" "s3://centreon-download/enterprises/centreon-bi-2/centreon-bi-3.1/centreon-bi-server-$VERSION/$SRCHASH/$PROJECT-$VERSION-php$phpversion.tar.gz"
  done
fi

# Generate online documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-bi-2 -V latest -p'"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-bi-2 -V latest -p'"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-bi-2 -V 3.1.x -p'"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-bi-2 -V 3.1.x -p'"
