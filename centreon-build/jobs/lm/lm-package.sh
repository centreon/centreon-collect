#!/bin/sh

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-license-manager

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
MAJOR=`echo $VERSION | cut -d . -f 1,2`

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Fetch sources.
if [ "$MAJOR" = "20.10" -o "$MAJOR" = "20.04" ] ; then
  PHPVERSION=php72
else
  PHPVERSION=php73
fi

rm -rf "$PROJECT-$VERSION-$PHPVERSION.tar.gz" "$PROJECT-$VERSION-$PHPVERSION"
get_internal_source "lm/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION-$PHPVERSION.tar.gz"
tar xzf "$PROJECT-$VERSION-$PHPVERSION.tar.gz"
OLDVERSION="$VERSION"
OLDRELEASE="$RELEASE"
PRERELEASE=`echo $VERSION | cut -d - -s -f 2-`
if [ -n "$PRERELEASE" ] ; then
  export VERSION=`echo $VERSION | cut -d - -f 1`
  export RELEASE="$PRERELEASE.$RELEASE"
  rm -rf "$PROJECT-$VERSION-$PHPVERSION"
  mv "$PROJECT-$OLDVERSION-$PHPVERSION" "$PROJECT-$VERSION-$PHPVERSION"
  tar czf "$PROJECT-$VERSION-$PHPVERSION.tar.gz" "$PROJECT-$VERSION-$PHPVERSION"
fi

# Retrieve sources.
cp "$PROJECT-$VERSION-$PHPVERSION.tar.gz" input/"$PROJECT-$VERSION-$PHPVERSION.tar.gz"
cp "$PROJECT-$VERSION-$PHPVERSION/packaging/$PROJECT.spectemplate" input/

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-$MAJOR:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG" input output
export VERSION="$OLDVERSION"
export RELEASE="$OLDRELEASE"

# Create RPMs tarball.
tar czf "rpms-$DISTRIB.tar.gz" output
