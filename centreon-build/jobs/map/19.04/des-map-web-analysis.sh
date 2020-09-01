#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/map-server/$PROJECT-server-$VERSIONSERVER-$RELEASE/$PROJECT-git.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT/web"
git checkout sonar-project.properties
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-map-web-19.04/centreon-map-web-19.04-release/g' sonar-project.properties
  sed -i -e 's/Centreon Map Web 19.04/Centreon Map Web 19.04 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSIONWEB" >> sonar-project.properties
sonar-scanner
