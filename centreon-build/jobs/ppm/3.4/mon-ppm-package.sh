#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-pp-manager

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
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
get_internal_source "ppm/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Encrypt source archive.
if [ "$DISTRIB" = "centos7" ] ; then
  phpversion=54
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
curl -F "file=@$PROJECT-$VERSION.tar.gz" -F "version=$phpversion" -F "modulename=$PROJECT" -F 'needlicense=0' 'http://encode.int.centreon.com/api/' -o "input/$PROJECT-$VERSION-php$phpversion.tar.gz"

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-3.4:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
cp "$PROJECT-$VERSION/packaging/$PROJECT.spectemplate" input
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output

# Copy files to server.
if [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIB='el7'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
put_internal_rpms "3.4" "$DISTRIB" "noarch" "ppm" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
