#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after des-bam-unittest on centos7.

# Project.
PROJECT=centreon-bam-server

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/bam/$PROJECT-$VERSION-$RELEASE/$PROJECT-git.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-bam-19.10/centreon-bam-19.10-release/g' sonar-project.properties
  sed -i -e 's/Centreon BAM 19.10/Centreon BAM 19.10 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
