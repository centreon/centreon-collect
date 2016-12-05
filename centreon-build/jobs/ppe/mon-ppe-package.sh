#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Generate archive of Centreon PPE.
cd centreon-export
git archive --prefix="centreon-export/" "$GIT_BRANCH" | gzip > "../centreon-export.centos$CENTOS_VERSION.tar.gz"
cd ..

# Copy files to server.
FILES="centreon-export.centos$CENTOS_VERSION.tar.gz"
scp -o StrictHostKeyChecking=no $FILES "ubuntu@srvi-repo.int.centreon.com:/srv/sources/"
