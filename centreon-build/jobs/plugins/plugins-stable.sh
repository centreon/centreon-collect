#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
export PROJECT=centreon-plugins

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/standard/testing/plugins/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Put sources online.
SRCHASH=`$SSH_REPO "md5sum /srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz | cut -d ' ' -f 1"`
$SSH_REPO aws s3 cp --acl public-read "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz" "s3://centreon-download/public/$PROJECT/$PROJECT-$VERSION.tar.gz"
curl "$DL_URL/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=0"

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "standard" "3.4" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "19.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "19.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "20.10" "el8" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "standard" "21.04" "el8" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"

# Move cache files to the stable directory.
TESTINGCACHE="/srv/cache/plugins/testing/cache-$VERSION-$RELEASE"
STABLECACHE="/srv/cache/plugins/stable"
$SSH_REPO 'for i in `ls '$TESTINGCACHE'` ; do rm -rf '$STABLECACHE'/$i ; mv '$TESTINGCACHE'/$i '$STABLECACHE'/ ; done ; rm -rf '$TESTINGCACHE
