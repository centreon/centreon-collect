#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-autodiscovery

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|...>"
  exit 1
fi
DISTRIB="$1"

# Frontend unit tests.
if [ "$DISTRIB" = 'frontend' ] ; then
  # Launch frontend unit test container.
  UT_IMAGE=registry.centreon.com/puppeteer:latest
  docker pull $UT_IMAGE
  containerid=`docker create $UT_IMAGE /usr/local/bin/unittest.sh $PROJECT`

  # Copy sources to container.
  docker cp `dirname $0`/autodisco-unittest.frontend.sh "$containerid:/usr/local/bin/unittest.sh"
  docker cp "$PROJECT" "$containerid:/usr/local/src/$PROJECT"

  # Run unit tests.
  rm -rf ut-fe.xml codestyle-fe.xml snapshots
  docker start -a "$containerid"
  docker cp "$containerid:/tmp/ut.xml" ut-fe.xml
  docker cp "$containerid:/tmp/codestyle.xml" codestyle-fe.xml
  docker cp "$containerid:/usr/local/src/$PROJECT/www/modules/$PROJECT/react/__image_snapshots__/__diff_output__" snapshots || true
else
  # Launch backend unit test container.
  UT_IMAGE=registry.centreon.com/mon-unittest-21.04:$DISTRIB
  docker pull "$UT_IMAGE"
  containerid=`docker create $UT_IMAGE /usr/local/bin/unittest.sh $PROJECT`

  # Copy sources to container.
  docker cp `dirname $0`/autodisco-unittest.backend.sh "$containerid:/usr/local/bin/unittest.sh"
  docker cp "$PROJECT" "$containerid:/usr/local/src/$PROJECT"

  # Run unit tests.
  rm -rf "ut-be-$DISTRIB.xml" "coverage-be-$DISTRIB.xml" "codestyle-be-$DISTRIB.xml" "phpstan-$DISTRIB.xml"
  docker start -a "$containerid"
  docker cp "$containerid:/tmp/ut-be.xml" "ut-be-$DISTRIB.xml"
  docker cp "$containerid:/tmp/coverage-be.xml" "coverage-be-$DISTRIB.xml"
  docker cp "$containerid:/tmp/codestyle-be.xml" "codestyle-be-$DISTRIB.xml"
  docker cp "$containerid:/tmp/phpstan.xml" "phpstan-$DISTRIB.xml"
fi

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
