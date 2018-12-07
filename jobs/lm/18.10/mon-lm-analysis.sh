#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after mon-web-unittest on centos7.

# Project.
PROJECT=centreon-license-manager

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/lm/$PROJECT-$VERSION-$RELEASE/$PROJECT-git.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT"
cp ../ut.xml .
cp ../coverage.xml .
sed -i -e 's#/usr/local/src/centreon-license-manager/##g' coverage.xml
sonar-scanner
