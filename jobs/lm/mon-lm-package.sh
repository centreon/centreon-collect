#!/bin/sh

set -e
set -x

# Generate archive of Centreon LM.
cd centreon-license-manager
git archive --prefix="centreon-license-manager/" "$GIT_BRANCH" | gzip > "../centreon-license-manager.tar.gz"
cd ..

# Copy files to server.
FILES="centreon-license-manager.tar.gz"
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.merethis.net:/tmp/"
