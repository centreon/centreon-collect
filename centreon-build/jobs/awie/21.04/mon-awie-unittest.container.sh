#!/bin/sh

#
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
#

set -x

# Get project.
PROJECT=centreon-awie

# Remove old reports.
rm -f /tmp/codestyle-be.xml

# Install dependencies.
chown -R root:root "/usr/local/src/$PROJECT"
cd "/usr/local/src/$PROJECT"
composer install

# Prepare build directory
mkdir -p build

# Run backend unit tests and code style.
composer run-script codestyle:ci

# Move reports to expected places.
mv build/checkstyle.xml /tmp/codestyle-be.xml
