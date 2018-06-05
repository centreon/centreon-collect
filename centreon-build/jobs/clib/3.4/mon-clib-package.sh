#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-clib

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1;
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7|debian9|...>"
  exit 1
fi
DISTRIB="$1"

# Pull mon-build-dependencies container.
if [ "$DISTRIB" = "centos6" ] ; then
  docker pull ci.int.centreon.com:5000/mon-build-dependencies-3.4:$DISTRIB
else
  docker pull ci.int.centreon.com:5000/mon-build-dependencies-3.5:$DISTRIB
fi

# Retrieve sources.
get_internal_source "clib/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"

# Run distribution-dependent script.
. `dirname $0`/mon-clib-package.$DISTRIB.sh
