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
  echo "USAGE: $0 <centos6|centos7|debian9|opensuse423>"
  exit 1
fi
DISTRIB="$1"

# Pull mon-build-dependencies container.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB

# Retrieve sources.
get_internal_source "broker/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz"

# Retrieve packaging files.
if [ \! -d packaging-centreon-broker ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon-broker.git
else
  cd packaging-centreon-broker
  git pull
  cd ..
fi

# Run distribution-dependent script.
. `dirname $0`/mon-broker-package.$DISTRIB.sh
