#!/bin/sh

#
# Copyright 2005 - 2020 Centreon (https://www.centreon.com/)
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

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-widget-$WIDGET
READABLE_NAME=$(echo "$PROJECT" | sed -e 's/-/ /g' -e 's/\b\(.\)/\u\1/g')

# Retrieve copy of git repository.
curl -o "$PROJECT-git.tar.gz" "http://srvi-repo.int.centreon.com/sources/internal/widget-$WIDGET/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
rm -rf "$PROJECT"
tar xzf "$PROJECT-git.tar.gz"

# Copy reports and run analysis.
cd "$PROJECT-$VERSION"
cp ../ut-be.xml .
cp ../coverage-be.xml .
sed -i -e 's#/usr/local/src/$PROJECT/##g' coverage-be.xml
if [ "$BUILD" '=' 'RELEASE' ] ; then
  sed -i -e "s/"$PROJECT"-21.04/"$PROJECT"-21.04-release/g" sonar-project.properties
  sed -i -e "s/Centreon Widget "$READABLE_NAME" 21.04/Centreon Widget "$READABLE_NAME" 21.04 (release)/g" sonar-project.properties
fi
echo "sonar.projectVersion="$VERSION >> sonar-project.properties
sonar-scanner