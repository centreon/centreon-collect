#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-bi-engine

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move sources to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO mv "/srv/sources/mbi/testing/mbi-engine/$PROJECT-$VERSION-$RELEASE" "/srv/sources/mbi/stable/"

# Move RPMs to the stable repository.
promote_testing_rpms_to_stable "mbi" "18.10" "el7" "noarch" "mbi-engine" "$PROJECT-$VERSION-$RELEASE"
