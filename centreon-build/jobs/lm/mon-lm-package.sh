#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Generate archive of Centreon LM.
cd centreon-license-manager
git archive --prefix="centreon-license-manager/" "$GIT_BRANCH" | gzip > "../centreon-license-manager.centos$CENTOS_VERSION.tar.gz"
cd ..

# Copy files to server.
FILES="centreon-license-manager.centos$CENTOS_VERSION.tar.gz"
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.int.centreon.com:/tmp/"
