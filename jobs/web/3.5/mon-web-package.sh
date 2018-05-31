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
  echo "USAGE: $0 <centos6|centos7|debian9|opensuse-leap>"
  exit 1
fi
DISTRIB="$1"

# Pull mon-build-dependencies container.
docker pull ci.int.centreon.com:5000/mon-build-dependencies-3.5:$DISTRIB

# Retrieve sources.
get_internal_source "web/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"
rm -rf "$PROJECT-$VERSION" "centreon-$VERSION" "centreon-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
mv "$PROJECT-$VERSION" "centreon-$VERSION"
tar czf "centreon-$VERSION.tar.gz" "centreon-$VERSION"

# Retrieve packaging files.
if [ \! -d packaging-centreon-web ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon packaging-centreon-web
else
  cd packaging-centreon-web
  git pull
  cd ..
fi

# Run distribution-dependent script.
. `dirname $0`/mon-web-package.$DISTRIB.sh
