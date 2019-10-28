#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-gorgone

# Copy reports and run analysis.
cd "$PROJECT"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  cp sonar-project.properties ../
  sed -i -e 's/centreon-gorgone-20.04/centreon-gorgone-20.04-release/g' sonar-project.properties
  sed -i -e 's/Centreon Gorgone 20.04/Centreon Gorgone 20.04 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
if [ "$BUILD" '=' 'RELEASE' ] ; then
  mv ../sonar-project.properties .
fi
