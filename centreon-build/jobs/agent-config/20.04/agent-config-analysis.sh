#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after agent-config-source on centos7.

# Project.
PROJECT=centreon-agent-configuration

# Copy reports and run analysis.
cd "$PROJECT"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-agent-configuration-20.04/centreon-agent-configuration-20.04-release/g' sonar-project.properties
  sed -i -e 's/Centreon Agent Configuration 20.04/Centreon Agent Configuration 20.04 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
