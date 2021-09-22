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

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
cd input
get_internal_source "autodisco/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz" -C ../
cd ..

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-3.4:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
cp "$PROJECT-$VERSION/packaging/$PROJECT.spectemplate" input
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output --verbose

# Copy files to server.
if [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIB='el7'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
put_internal_rpms "3.4" "$DISTRIB" "noarch" "autodisco" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
