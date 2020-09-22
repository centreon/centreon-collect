#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-map-web-client

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh
$SSH_REPO /srv/scripts/sync-map.sh --confirm
