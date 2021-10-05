#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull mon-build-dependencies container.
docker pull registry.centreon.com/mon-build-dependencies-3.4:$DISTRIB

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Retrieve spec file and additional sources.
cp `dirname $0`/../../../packaging/web/rpm/3.4/centreon.spectemplate input/
cp `dirname $0`/../../../packaging/web/src/3.4/* input

# Retrieve sources.
cd input
get_internal_source "web/centreon-web-$VERSION-$RELEASE/centreon-$VERSION.tar.gz"
cd ..

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key registry.centreon.com/mon-build-dependencies-3.4:$DISTRIB input output

# Copy files to server.
DISTRIB='el7'

put_internal_rpms "3.4" "$DISTRIB" "noarch" "web" "centreon-web-$VERSION-$RELEASE" output/noarch/*.rpm
