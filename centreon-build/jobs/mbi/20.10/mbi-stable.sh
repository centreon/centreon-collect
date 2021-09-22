#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
export PROJECT=centreon-mbi

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "mbi" "20.10" "el7" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "mbi" "20.10" "el8" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE"

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/mbi/testing/mbi/$PROJECT-$VERSION-$RELEASE" "/srv/sources/mbi/stable/"

# Put sources online.
SRCHASH=`$SSH_REPO "cat /srv/sources/mbi/stable/$PROJECT-$VERSION-$RELEASE/centreon-bi-server-$VERSION-php72.tar.gz | md5sum | cut -d ' ' -f 1"`
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/mbi/stable/$PROJECT-$VERSION-$RELEASE/centreon-bi-server-$VERSION-php72.tar.gz" "s3://centreon-download/enterprises/centreon-mbi/centreon-mbi-20.10/centreon-mbi-$VERSION/$SRCHASH/centreon-bi-server-$VERSION-php72.tar.gz"

# Download link.
echo 'https://download.centreon.com/?action=product&product=centreon-mbi&version='$VERSION'&secKey='$SRCHASH
