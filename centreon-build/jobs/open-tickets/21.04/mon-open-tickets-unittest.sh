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

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-open-tickets

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|centos8|...>"
  exit 1
fi
DISTRIB="$1"

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "open-tickets/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Launch mon-unittest container.
UT_IMAGE=registry.centreon.com/mon-unittest-21.04:$DISTRIB
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest.sh $PROJECT`

# Copy sources to container.
docker cp `dirname $0`/mon-open-tickets-unittest.container.sh "$containerid:/usr/local/bin/unittest.sh"
docker cp "$PROJECT-$VERSION" "$containerid:/usr/local/src/$PROJECT"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/codestyle-be.xml" codestyle-be.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"