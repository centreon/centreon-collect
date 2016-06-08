#!/bin/sh

set -e
set -x

# Generate archive of Centreon IMP Portal API.
cd centreon-imp-portal-api
git archive --prefix="centreon-imp-portal-api/" "$GIT_BRANCH" | gzip > "../centreon-imp-portal-api.tar.gz"
cd ..

# Copy files to server.
FILES="centreon-imp-portal-api.tar.gz"
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.int.centreon.com:/tmp/"
