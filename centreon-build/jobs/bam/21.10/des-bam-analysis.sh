#!/bin/sh

. `dirname $0`/../../common.sh

# This job is run directly after des-bam-unittest on centos7.

# Project.
PROJECT=centreon-bam-server
PROJECT_NAME="Centreon Bam Server"

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/bam/$PROJECT-$VERSION-$RELEASE/$PROJECT-git.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT"
cp ../ut-be.xml .
cp ../coverage-be.xml .
sed -i -e 's#/usr/local/src/centreon-bam-server/##g' coverage-be.xml

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
