#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after agent-source on centos7.

# Project.
PROJECT=centreon-agent-

# Copy reports and run analysis.
cd "$PROJECT"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-agent/centreon-agent-release/g' sonar-project.properties
  sed -i -e 's/Centreon Agent/Centreon Agent (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
