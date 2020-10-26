#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-autodiscovery

# Copy reports and run analysis.
cd "$PROJECT"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-autodiscovery-21.04/centreon-autodiscovery-21.04-release/g' sonar-project.properties
  sed -i -e 's/Centreon Autodiscovery 21.04/Centreon Autodiscovery 21.04 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
