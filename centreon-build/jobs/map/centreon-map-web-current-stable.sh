#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-map-web-client

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh
$SSH_REPO /srv/scripts/sync-map.sh --confirm

# Update documentation
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-map-4 -V master -p ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-map-4 -V latest -p'"
