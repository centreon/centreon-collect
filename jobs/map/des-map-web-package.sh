#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-map-web
PACKAGE=centreon-map4-web-client

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
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
get_internal_source "map-web/$PACKAGE-$VERSION-$RELEASE/$PACKAGE-$VERSION.tar.gz"
get_internal_source "map-web/$PACKAGE-$VERSION-$RELEASE/$PACKAGE.spectemplate"
cd ..

# Pull latest build dependencies.
BUILD_IMG="ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG" input output

# Copy files to server.
if [ "$DISTRIB" = 'centos6' ] ; then
  DISTRIB='el6'
elif [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIB='el7'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
put_internal_rpms "3.4" "$DISTRIB" "noarch" "map-web" "$PACKAGE-$VERSION-$RELEASE" output/noarch/*.rpm
put_internal_rpms "3.5" "$DISTRIB" "noarch" "map-web" "$PACKAGE-$VERSION-$RELEASE" output/noarch/*.rpm
