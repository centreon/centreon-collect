#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
export PROJECT=centreon-bam-server

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/bam/testing/bam/$PROJECT-$VERSION-$RELEASE" "/srv/sources/bam/stable/"

# Put sources online.
SRCHASH=`$SSH_REPO "cat /srv/sources/bam/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-php72.tar.gz | md5sum | cut -d ' ' -f 1"`
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/bam/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-php72.tar.gz" "s3://centreon-download/enterprises/centreon-bam/centreon-bam-20.04/centreon-bam-$VERSION/$SRCHASH/$PROJECT-$VERSION-php$phpversion.tar.gz"

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "bam" "20.04" "el7" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE"

# Generate online documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-bam -V latest -p'"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-bam -V latest -p'"

# Download link.
echo 'https://download.centreon.com/?action=product&product=centreon-bam&version='$VERSION'&secKey='$SRCHASH
