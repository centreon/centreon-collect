#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map
PACKAGE=centreon-map-web-client

# Check arguments.
if [ -z "$VERSION" -o -z "$VERSIONWEB" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION, VERSIONWEB and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|centos8|...>"
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
get_internal_source "map-web/$PROJECT-web-$VERSIONWEB-$RELEASE/$PACKAGE-$VERSIONWEB-php72.tar.gz"
get_internal_source "map-web/$PROJECT-web-$VERSIONWEB-$RELEASE/$PACKAGE.spectemplate"
cd ..

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-20.10:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
OLDVERSION="$VERSION"
export VERSION="$VERSIONWEB"
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output
export VERSION="$OLDVERSION"

tar czf rpms-$DISTRIB.tar.gz output