#!/bin/sh

# This job is run directly after mon-web-unittest on centos7.

curl -o 'centreon-web-git.tar.gz' "http://srvi-repo.int.centreon.com/sources/internal/web/$PROJECT-$VERSION-$RELEASE/centreon-web-git.tar.gz"
rm -rf centreon-web
tar xzf centreon-web-git.tar.gz
cd centreon-web
cp ../ut.xml .
cp ../coverage.xml .
sonar-scanner
