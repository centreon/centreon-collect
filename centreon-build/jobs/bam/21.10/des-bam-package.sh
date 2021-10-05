#!/bin/sh

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-bam-server

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
rm -rf "$PROJECT-$VERSION"
tar xzf "$PROJECT-$VERSION.tar.gz"
rm -rf "$PROJECT-$VERSION-php80"
tar xzf "$PROJECT-$VERSION-php80.tar.gz"
OLDVERSION="$VERSION"
OLDRELEASE="$RELEASE"
PRERELEASE=`echo $VERSION | cut -d - -s -f 2-`
if [ -n "$PRERELEASE" ] ; then
  export VERSION=`echo $VERSION | cut -d - -f 1`
  export RELEASE="$PRERELEASE.$RELEASE"
  rm -rf "$PROJECT-$VERSION-php80"
  mv "$PROJECT-$OLDVERSION-php80" "$PROJECT-$VERSION-php80"
  tar czf "$PROJECT-$VERSION-php80.tar.gz" "$PROJECT-$VERSION-php80"
fi

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
cp "$PROJECT-$VERSION-php80.tar.gz" input/"$PROJECT-$VERSION-php80.tar.gz"
cp "$PROJECT-$OLDVERSION/packaging/$PROJECT.spectemplate" input

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-21.10:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output

# Create RPMs tarball.
tar czf "rpms-$DISTRIB.tar.gz" output
