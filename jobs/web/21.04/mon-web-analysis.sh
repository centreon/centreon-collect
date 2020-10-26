#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after mon-web-unittest on centos7.

# Project.
PROJECT=centreon-web

# Copy reports and run analysis.
cd centreon-web
cp ../ut-be.xml .
cp ../coverage-be.xml .
sed -i -e 's#/usr/local/src/centreon-web/##g' coverage-be.xml
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-web-21.04/centreon-web-21.04-release/g' sonar-project.properties
  sed -i -e 's/Centreon Web 21.04/Centreon Web 21.04 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
