#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map
PACKAGE=centreon-map4-web-client

# Check arguments.
if [ -z "$VERSION" -o -z "$VERSIONWEB" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION, VERSIONWEB and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|...>"
  exit 1
fi
DISTRIB="$1"

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Fetch sources.
cd input
get_internal_source "map/$PROJECT-$VERSION-$RELEASE/$PACKAGE-$VERSIONWEB.tar.gz"
get_internal_source "map/$PROJECT-$VERSION-$RELEASE/$PACKAGE.spectemplate"
cd ..

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-3.4:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
OLDVERSION="$VERSION"
export VERSION="$VERSIONWEB"
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output
export VERSION="$OLDVERSION"

# Copy files to server.
if [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIB='el7'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
put_internal_rpms "3.4" "$DISTRIB" "noarch" "map" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
