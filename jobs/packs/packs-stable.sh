#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
export PROJECT=centreon-packs

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/plugin-packs/testing/packs/$PROJECT-$VERSION-$RELEASE" "/srv/sources/plugin-packs/stable/"

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "plugin-packs" "3.4" "el6" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "3.4" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "19.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "19.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "20.04" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "20.10" "el7" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"
promote_testing_rpms_to_stable "plugin-packs" "20.10" "el8" "noarch" "packs" "$PROJECT-$VERSION-$RELEASE"

# Move cache files to the stable directory.
TESTINGCACHE="/srv/cache/packs/testing/cache-$VERSION-$RELEASE"
STABLECACHE="/srv/cache/packs/stable"
$SSH_REPO 'for i in `ls '$TESTINGCACHE'` ; do rm -rf '$STABLECACHE'/$i ; mv '$TESTINGCACHE'/$i '$STABLECACHE'/ ; done ; rm -rf '$TESTINGCACHE
