#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-autodiscovery

# Copy reports and run analysis.
cd "$PROJECT"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-autodiscovery-20.10/centreon-autodiscovery-20.10-release/g' sonar-project.properties
  sed -i -e 's/Centreon Autodiscovery 20.10/Centreon Autodiscovery 20.10 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
