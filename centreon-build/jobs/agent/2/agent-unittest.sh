#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-agent

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
rm -rf github.com
mkdir -p github.com/centreon
cd github.com/centreon
tar xzf "../../$PROJECT-$VERSION.tar.gz"
mv "$PROJECT-$VERSION" "$PROJECT"
cd ../..

# Prepare unit test container.
UT_IMAGE=registry.centreon.com/build-dependencies-agent-2:$DISTRIB
docker pull "$UT_IMAGE"
containerid=`docker create $UT_IMAGE /usr/local/bin/unittest.sh`

# Copy sources to container.
docker cp `dirname $0`/agent-unittest.container.sh "$containerid:/usr/local/bin/unittest.sh"
docker cp "github.com/." "$containerid:/root/go/src/github.com/"

# Run unit tests.
docker start -a "$containerid"
docker cp "$containerid:/tmp/ut.xml" ut.xml

# Stop container.
docker stop "$containerid"
docker rm "$containerid"
