#!/bin/sh

set -e
set -x

# Generate archive of Centreon PPE.
cd centreon-export
git archive --prefix="centreon-export/" "$GIT_BRANCH" | gzip > "../centreon-export.tar.gz"
cd ..

# Copy files to server.
FILES="centreon-export.tar.gz"
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.merethis.net:/tmp/"
