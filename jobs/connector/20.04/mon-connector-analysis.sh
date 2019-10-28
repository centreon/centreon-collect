#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-connector

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/connector/$PROJECT-$VERSION-$RELEASE/$PROJECT-git.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-connector-20.04/centreon-connector-20.04-release/g' sonar-project.properties
  sed -i -e 's/Centreon Connector 20.04/Centreon Connector 20.04 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
