#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-web

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
docker pull registry.centreon.com/mon-build-dependencies-20.04:$DISTRIB

# Retrieve sources.
rm -rf "$PROJECT-$VERSION" "centreon-$VERSION" "centreon-$VERSION.tar.gz"
get_internal_source "web/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
export THREEDIGITVERSION=`echo $VERSION | cut -d - -f 1`
rm -rf "centreon-$THREEDIGITVERSION" "centreon-$THREEDIGITVERSION.tar.gz"
mv "$PROJECT-$VERSION" "centreon-$THREEDIGITVERSION"
tar czf "centreon-$THREEDIGITVERSION.tar.gz" "centreon-$THREEDIGITVERSION"

# Run distribution-dependent script.
. `dirname $0`/mon-web-package.$DISTRIB.sh
