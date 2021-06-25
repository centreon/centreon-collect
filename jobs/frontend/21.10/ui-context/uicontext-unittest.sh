#!/bin/sh

set -e
set -x

. `dirname $0`/../../../common.sh

# Project.
PROJECT=ui-context
PROJECT_DIR="centreon-frontend/packages/ui-context"

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Launch mon-unittest container.
UT_IMAGE=registry.centreon.com/puppeter:node-16
docker pull $UT_IMAGE
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest.sh $PROJECT`

# Copy sources to container.
docker cp `dirname $0`/uicontext-unittest.container.sh "$containerid:/usr/local/bin/unittest.sh"
docker cp "centreon-frontend" "$containerid:/usr/local/"

# Run unit tests.
rm -rf codestyle.xml 
docker start -a "$containerid"
docker cp "$containerid:/tmp/codestyle.xml" codestyle.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid" 
