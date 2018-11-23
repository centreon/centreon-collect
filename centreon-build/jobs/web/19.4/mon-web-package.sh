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
docker pull ci.int.centreon.com:5000/mon-build-dependencies-19.4:$DISTRIB

# Retrieve sources.
rm -rf "$PROJECT-$VERSION" "centreon-$VERSION" "centreon-$VERSION.tar.gz"
get_internal_source "web/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
mv "$PROJECT-$VERSION" "centreon-$VERSION"
tar czf "centreon-$VERSION.tar.gz" "centreon-$VERSION"

# Retrieve packaging files.
rm -rf packaging-centreon-web
git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon packaging-centreon-web
cd packaging-centreon-web
git checkout "$BRANCH_NAME" || true
cd ..

# Run distribution-dependent script.
. `dirname $0`/mon-web-package.$DISTRIB.sh
