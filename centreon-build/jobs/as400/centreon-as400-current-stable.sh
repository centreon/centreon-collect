#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the stable repository.
for project in centreon-as400-connector centreon-as400-connector-endoflife centreon-as400-plugin centreon-as400-plugin-endoflife ; do
  export PROJECT="$project"
  `dirname $0`/../testing-to-stable.sh
done
`dirname $0`/../sync-repo.sh --project plugin-packs --confirm

# Generate online documentation.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-as400 -V latest -p'"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-as400 -V latest -p'"
