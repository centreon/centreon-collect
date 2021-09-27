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
rm -rf "$PROJECT-$VERSION-php80.tar.gz" "$PROJECT-$VERSION-php80"
get_internal_source "ppe/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-php80.tar.gz"
tar xzf "$PROJECT-$VERSION-php80.tar.gz"

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
cp "$PROJECT-$VERSION-php80.tar.gz" input/"$PROJECT-$VERSION-php80.tar.gz"
cp "$PROJECT-$VERSION-php80/packaging/$PROJECT.spectemplate" input/

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-21.10:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output
tar czf rpms-$DISTRIB.tar.gz output