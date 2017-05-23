#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" ] ; then
  echo "You need to specify VERSION environment variable."
  exit 1
fi

# Move artifacts to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/map/testing/centreon-map-client-$VERSION" "/srv/sources/map/stable/"

# Copy p2 artifacts to remote server.
$SSH_REPO ssh -o StrictHostKeyChecking=no "map-repo@10.24.1.107" rm -rf "centreon-studio-repository/4/1"
$SSH_REPO scp -r "/srv/p2/testing/4/1" "map-repo@10.24.1.107:centreon-studio-repository/4/1"

# Generate online documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-map-4 -V latest -p'"

# Synchronize RPMs.
$SSH_REPO /srv/scripts/sync-map.sh --confirm
