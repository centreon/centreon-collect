#!/bin/sh

#
#
# Copyright 2005 - 2021 Centreon (https://www.centreon.com/)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#
#

set -e
set -x

. `dirname $0`/../../common.sh

# This job is run directly after mbi-unittest on centos7.

# Project.
PROJECT=centreon-mbi

# Retrieve copy of git repository.
echo "Downloading tarball"
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/mbi/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-full.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e 's/centreon-mbi-21.04/centreon-mbi-21.04-release/g' sonar-project.properties
  sed -i -e 's/Centreon MBI 21.04/Centreon MBI 21.04 (release)/g' sonar-project.properties
fi
echo "sonar.projectVersion=$VERSION" >> sonar-project.properties
sonar-scanner
