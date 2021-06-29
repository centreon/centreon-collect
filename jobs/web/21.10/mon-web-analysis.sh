#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after mon-web-unittest on centos7.

# Project.
PROJECT=centreon-web
PROJECT_NAME="Centreon Web"

# Copy reports and run analysis.
cd centreon-web
#cp ../ut-be.xml .
#cp ../coverage-be.xml .
#sed -i -e 's#/usr/local/src/centreon-web/##g' coverage-be.xml

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

# override missing AMI requirement
sudo apt-get install shellcheck
# override missing AMI requirement
sudo apt-get install shellcheck
sonar-scanner
