#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map

# Copy reports and run analysis.
cd "$PROJECT-desktop-$VERSION"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-map-desktop-20.04/centreon-map-desktop-20.04-release/g' sonar-project.properties
  sed -i -e 's/Centreon Map Desktop 20.04/Centreon Map Desktop 20.04 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
