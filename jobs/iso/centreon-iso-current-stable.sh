#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-iso

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Put sources online.
$SSH_REPO aws s3 cp --acl public-read "/srv/iso/centreon-$VERSION.$RELEASE-el6-x86_64.iso" "s3://centreon-iso/stable/$VERSION/centreon-$VERSION.$RELEASE-x86_64.iso"
