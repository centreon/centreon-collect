#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
export PROJECT=centreon-web

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE" "/srv/sources/standard/stable/"

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh
`dirname $0`/../sync-repo.sh --project lts --path /3.4 --confirm
