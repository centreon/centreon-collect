#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-broker

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
docker pull registry.centreon.com/mon-build-dependencies-3.4:$DISTRIB

# Retrieve sources.
get_internal_source "broker/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"

# Run distribution-dependent script.
. `dirname $0`/mon-broker-package.$DISTRIB.sh
