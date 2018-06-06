#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-discovery-engine

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1;
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|...>"
  exit 1
fi
DISTRIB="$1"

# Pull mon-build-dependencies container.
docker pull ci.int.centreon.com:5000/mon-build-dependencies-3.5:$DISTRIB

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve sources.
cd input
get_internal_source "automation-broker/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
cd ..
tar xzf "input/$PROJECT-$VERSION.tar.gz"
cp "$PROJECT-$VERSION/packaging/$PROJECT.spectemplate" input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key ci.int.centreon.com:5000/mon-build-dependencies-3.5:$DISTRIB input output

# Copy files to server.
DISTRIB='el7'
put_internal_rpms "3.5" "$DISTRIB" "x86_64" "automation-broker" "$PROJECT-$VERSION-$RELEASE" output/x86_64/*.rpm
