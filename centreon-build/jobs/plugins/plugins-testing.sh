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
$SSH_REPO mv "/srv/sources/standard/unstable/plugins/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/testing/plugins/"

# Move RPMs to the testing repository.
promote_unstable_rpms_to_testing "standard" "3.4" "el6" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "3.4" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"
promote_unstable_rpms_to_testing "standard" "18.10" "el7" "noarch" "plugins" "$PROJECT-$VERSION-$RELEASE"

# Move cache files to the testing directory.
ssh "$REPO_CREDS" mv "/srv/cache/plugins/unstable/cache-$VERSION-$RELEASE" "/srv/cache/plugins/testing/"

# Generate doc.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-plugins -V latest -p'"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-plugins -V latest -p'"
