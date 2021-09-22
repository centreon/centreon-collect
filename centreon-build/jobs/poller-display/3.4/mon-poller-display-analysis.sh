#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after mon-poller-display-unittest on centos7.

# Project.
PROJECT=centreon-poller-display

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/poller-display/$PROJECT-$VERSION-$RELEASE/$PROJECT-git.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT"
cp ../ut.xml .
cp ../coverage.xml .
sed -i -e 's#/usr/local/src/centreon-poller-display/##g' coverage.xml

sonar-scanner
