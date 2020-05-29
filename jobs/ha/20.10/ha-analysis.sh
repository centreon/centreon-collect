#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after ha-source on centos7.

# Project.
PROJECT=centreon-ha

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/ha/$PROJECT-$VERSION-$RELEASE/$PROJECT-git.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-ha-20.10/centreon-ha-20.10-release/g' sonar-project.properties
  sed -i -e 's/Centreon HA 20.10/Centreon HA 20.10 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
