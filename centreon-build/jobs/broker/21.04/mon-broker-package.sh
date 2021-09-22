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
docker pull registry.centreon.com/mon-build-dependencies-21.04:$DISTRIB

# Retrieve sources.
export THREEDIGITVERSION=`echo $VERSION | cut -d - -f 1`
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz" "$PROJECT-$THREEDIGITVERSION"
get_internal_source "broker/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
if [ "$THREEDIGITVERSION" '!=' "$VERSION" ] ; then
  tar xzf "$PROJECT-$VERSION.tar.gz"
  mv "$PROJECT-$VERSION" "$PROJECT-$THREEDIGITVERSION"
  tar czf "$PROJECT-$THREEDIGITVERSION.tar.gz" "$PROJECT-$THREEDIGITVERSION"
fi

# Run distribution-dependent script.
. `dirname $0`/mon-broker-package.$DISTRIB.sh
