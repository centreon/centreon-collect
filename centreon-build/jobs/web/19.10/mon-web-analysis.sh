#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after mon-web-unittest on centos7.

# Project.
PROJECT=centreon-web
PROJECT_NAME="Centreon Web"

# Retrieve copy of git repository.
curl -o 'centreon-web-git.tar.gz' "http://srvi-repo.int.centreon.com/sources/internal/web/$PROJECT-$VERSION-$RELEASE/centreon-web-git.tar.gz"
rm -rf centreon-web
tar xzf centreon-web-git.tar.gz

# Copy reports and run analysis.
cd centreon-web
cp ../ut.xml .
cp ../coverage.xml .
sed -i -e 's#/usr/local/src/centreon-web/##g' coverage.xml

# environment values required to replace sonarQube project versioning and binding
#   sonar.projectKey="{PROJECT_TITLE}"
#   sonar.projectName="{PROJECT_NAME}"
#   sonar.projectKey="{PROJECT_VERSION}"
echo "BRANCH_NAME      -> $BRANCH_NAME"
echo "PROJECT_TITLE    -> $PROJECT"
echo "PROJECT_NAME     -> $PROJECT_NAME"
echo "PROJECT_VERSION  -> $VERSION"
sed -i -e "s/{PROJECT_TITLE}/$PROJECT/g" sonar-project.properties
sed -i -e "s/{PROJECT_NAME}/$PROJECT_NAME/g" sonar-project.properties
sed -i -e "s/{PROJECT_VERSION}/$VERSION/g" sonar-project.properties

sonar-scanner
