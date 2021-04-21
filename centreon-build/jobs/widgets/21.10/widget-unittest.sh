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

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" -o -z "$WIDGET" ] ; then
  echo "You need to specify VERSION, RELEASE and WIDGET environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|centos8|...>"
  exit 1
fi
DISTRIB="$1"
case "$DISTRIB" in
  centos7)
    DISTRIBCODENAME=el7
    ;;
  centos8)
    DISTRIBCODENAME=el8
    ;;
  *)
    echo "Unsupported distribution $DISTRIB"
    exit 1
esac

# Project.
PROJECT=centreon-widget-$WIDGET

# Fetch sources.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "widget-"$WIDGET"/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Launch mon-unittest container.
UT_IMAGE=registry.centreon.com/mon-unittest-21.10:$DISTRIB
docker pull $UT_IMAGE
containerid=`docker create --env WIDGET_NAME=$WIDGET $UT_IMAGE /usr/local/bin/unittest.sh $PROJECT`

# Copy sources to container.
docker cp `dirname $0`/widget-unittest.container.sh "$containerid:/usr/local/bin/unittest.sh"
docker cp "$PROJECT-$VERSION" "$containerid:/usr/local/src/$PROJECT"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/codestyle-be.xml" codestyle-be.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
