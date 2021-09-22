#!/bin/sh

set -e
set -x

. `dirname $0`/../../../common.sh

# Project.
PROJECT=centreon-ui
PROJECT_DIR="centreon-frontend/packages/centreon-ui"

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
docker cp `dirname $0`/centreonui-unittest.container.sh "$containerid:/usr/local/bin/unittest.sh"
docker cp "centreon-frontend" "$containerid:/usr/local/"

# Run unit tests.
rm -rf ut.xml codestyle.xml snapshots storybook
docker start -a "$containerid"
docker cp "$containerid:/tmp/ut.xml" ut.xml
docker cp "$containerid:/tmp/codestyle.xml" codestyle.xml
docker cp "$containerid:/usr/local/centreon-frontend/packages/centreon-ui/src/__image_snapshots__/__diff_output__" snapshots || true

# Store storybook.
docker cp "$containerid:/usr/local/centreon-frontend/packages/centreon-ui/.out" storybook
put_internal_source "frontend" "$PROJECT-$VERSION-$RELEASE" "storybook"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
