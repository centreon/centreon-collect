#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Generate archive of Centreon IMP Portal API.
cd centreon-imp-portal-api
git archive --prefix="centreon-imp-portal-api/" "$GIT_BRANCH" | gzip > "../centreon-imp-portal-api.centos$CENTOS_VERSION.tar.gz"
cd ..

# Copy files to server.
FILES="centreon-imp-portal-api.centos$CENTOS_VERSION.tar.gz"
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.merethis.net:/tmp/"
