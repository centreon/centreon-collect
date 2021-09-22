#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-export

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

# Fetch sources.
rm -rf "$PROJECT-$VERSION-php73.tar.gz" "$PROJECT-$VERSION-php73"
get_internal_source "ppe/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-php73.tar.gz"
tar xzf "$PROJECT-$VERSION-php73.tar.gz"

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
cp "$PROJECT-$VERSION-php73.tar.gz" input/"$PROJECT-$VERSION-php73.tar.gz"
cp "$PROJECT-$VERSION-php73/packaging/$PROJECT.spectemplate" input/

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-21.04:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output

# Copy files to server.
if [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIB='el7'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
put_internal_rpms "21.04" "$DISTRIB" "noarch" "ppe" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
