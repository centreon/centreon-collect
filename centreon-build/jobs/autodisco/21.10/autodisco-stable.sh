#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
export PROJECT=centreon-autodiscovery

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/standard/testing/autodisco/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Put sources online.
SRCHASH=`$SSH_REPO "md5sum /srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-php73.tar.gz | cut -d ' ' -f 1"`
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-php73.tar.gz" "s3://centreon-download/public/$PROJECT/$PROJECT-$VERSION-php73.tar.gz"
curl "$DL_URL/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION-php73&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=0"

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "standard" "21.10" "el7" "noarch" "autodisco" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.10" "el8" "noarch" "autodisco" "$PROJECT-$VERSION-$RELEASE"
