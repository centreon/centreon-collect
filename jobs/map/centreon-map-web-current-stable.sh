#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-map-web-client

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Move RPMs to the stable repository.
`dirname $0`/../testing-to-stable.sh
$SSH_REPO /srv/scripts/sync-map.sh --confirm
