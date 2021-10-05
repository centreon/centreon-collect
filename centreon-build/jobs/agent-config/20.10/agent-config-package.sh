#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-agent-configuration

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

# Fetch sources.
tar xzf "$PROJECT-$VERSION.tar.gz"
OLDVERSION="$VERSION"
OLDRELEASE="$RELEASE"
PRERELEASE=`echo $VERSION | cut -d - -s -f 2-`
if [ -n "$PRERELEASE" ] ; then
  export VERSION=`echo $VERSION | cut -d - -f 1`
  export RELEASE="$PRERELEASE.$RELEASE"
  rm -rf "$PROJECT-$VERSION"
  mv "$PROJECT-$OLDVERSION" "$PROJECT-$VERSION"
  tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
fi

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-20.10:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
cp "$PROJECT-$VERSION.tar.gz" input/
cp `dirname $0`"/../../../packaging/agent-config/20.10/$PROJECT.spectemplate" input/
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output
export VERSION="$OLDVERSION"
export RELEASE="$OLDRELEASE"

# Copy files to server.
if [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIB=el7
elif [ "$DISTRIB" = 'centos8' ] ; then
  DISTRIB=el8
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
put_internal_rpms "20.10" "$DISTRIB" "noarch" "agent-config" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "standard" "20.10" "$DISTRIB" "noarch" "agent-config" "$PROJECT-$VERSION-$RELEASE"
fi
