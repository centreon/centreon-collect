#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-hub-ui
PROJECT_NAME="Centreon Hub UI"

# Run analysis.
cd "$PROJECT"

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
