#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

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

# Pull mon-build-dependencies container.
docker pull registry.centreon.com/mon-build-dependencies-21.10:$DISTRIB

# Create and populate container.
BUILD_IMAGE="registry.centreon.com/mon-build-dependencies-21.10:centos7"
docker pull $BUILD_IMAGE
containerid=`docker create -e "PROJECT=$PROJECT" -e "VERSION=$VERSION" $BUILD_IMAGE /usr/local/bin/package.sh`
docker cp `dirname $0`/mon-web-package.container.sh "$containerid:/usr/local/bin/package.sh"
docker cp "$PROJECT-$VERSION.tar.gz" "$containerid:/usr/local/src/"

# Run container that will generate complete tarball.
docker start -a "$containerid"
rm -f "$PROJECT-$VERSION.tar.gz"
docker cp "$containerid:/usr/local/src/$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION.tar.gz"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Retrieve sources.
rm -rf "$PROJECT-$VERSION"
tar xzf "$PROJECT-$VERSION.tar.gz"
export THREEDIGITVERSION=`echo $VERSION | cut -d - -f 1`
rm -rf "centreon-$THREEDIGITVERSION" "centreon-$THREEDIGITVERSION.tar.gz"
mv "$PROJECT-$VERSION" "centreon-$THREEDIGITVERSION"
tar czf "centreon-$THREEDIGITVERSION.tar.gz" "centreon-$THREEDIGITVERSION"

# Run distribution-dependent script.
. `dirname $0`/mon-web-package.$DISTRIB.sh
