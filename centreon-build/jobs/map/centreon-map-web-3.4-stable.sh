#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-map-web-client

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh
`dirname $0`/../sync-repo.sh --project map --confirm
