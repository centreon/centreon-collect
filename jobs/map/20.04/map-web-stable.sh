#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "map" "20.04" "el7" "noarch" "map-web" "$PROJECT-web-$VERSION-$RELEASE"

# Update documentation
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-map-4 -V latest -p'"
