#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-engine

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

# Pull mon-build-dependencies container.
docker pull registry.centreon.com/mon-build-dependencies-19.10:$DISTRIB

# Retrieve sources.
export THREEDIGITVERSION=`echo $VERSION | cut -d - -f 1`
rm -rf "$PROJECT-$VERSION" "$PROJECT-$VERSION.tar.gz" "$PROJECT-$THREEDIGITVERSION"
get_internal_source "engine/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
mv "$PROJECT-$VERSION" "$PROJECT-$THREEDIGITVERSION"
tar czf "$PROJECT-$THREEDIGITVERSION.tar.gz" "$PROJECT-$THREEDIGITVERSION"

# Run distribution-dependent script.
. `dirname $0`/mon-engine-package.$DISTRIB.sh
