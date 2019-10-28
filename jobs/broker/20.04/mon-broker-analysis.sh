#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after mon-broker-unittest on centos7.

# Project.
PROJECT=centreon-broker

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/broker/$PROJECT-$VERSION-$RELEASE/$PROJECT-git.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT"
cp ../ut.xml .
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-broker-20.04/centreon-broker-20.04-release/g' sonar-project.properties
  sed -i -e 's/Centreon Broker 20.04/Centreon Broker 20.04 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
