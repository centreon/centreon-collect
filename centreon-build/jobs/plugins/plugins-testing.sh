#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-plugins

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the testing directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/internal/plugins/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/testing/plugins/"

# Move RPMs to the testing repository.
promote_unstable_rpms_to_testing "standard" "3.4" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "19.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "19.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "20.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "20.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "20.10" "el8" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "21.04" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "21.04" "el8" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"

# Move cache files to the testing directory.
ssh "$REPO_CREDS" mv "/srv/cache/plugins/unstable/cache-$VERSION-$RELEASE" "/srv/cache/plugins/testing/"
