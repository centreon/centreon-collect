#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after mon-ppm-unittest on centos7.

# Project.
PROJECT=centreon-pp-manager

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/ppm/$PROJECT-$VERSION-$RELEASE/$PROJECT-git.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT"
# override missing AMI requirement
sudo apt-get install shellcheck
sonar-scanner
