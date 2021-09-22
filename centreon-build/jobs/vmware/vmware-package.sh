#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-vmware
PKGNAME=centreon-plugin-Virtualization-VMWare-daemon

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|centos8|...>"
  exit 1
fi
DISTRIB="$1"

# Pull mon-build-dependencies container.
if [ "$DISTRIB" '=' 'centos7' ] ; then
  docker pull registry.centreon.com/mon-build-dependencies-19.04:$DISTRIB
else
  docker pull registry.centreon.com/mon-build-dependencies-20.10:$DISTRIB
fi

# Retrieve sources.
rm -rf "$PKGNAME-$VERSION" "$PKGNAME-$VERSION.tar.gz"
get_internal_source "vmware/$PROJECT-$VERSION-$RELEASE/$PKGNAME-$VERSION.tar.gz"

# Run distribution-dependent script.
. `dirname $0`/vmware-package.$DISTRIB.sh
