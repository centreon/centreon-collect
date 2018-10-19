#!/bin/sh

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO /srv/scripts/sync-$REPO.sh
