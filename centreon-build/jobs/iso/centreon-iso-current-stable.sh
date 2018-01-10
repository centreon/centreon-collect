#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-iso
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Put sources online.
SRCHASH=`$SSH_REPO "md5sum /srv/iso/centreon-$VERSION.$RELEASE-el6-x86_64.iso | cut -d ' ' -f 1"`
$SSH_REPO aws s3 cp --acl public-read "/srv/iso/centreon-$VERSION.$RELEASE-el6-x86_64.iso" "s3://centreon-iso/stable/$VERSION/centreon-$VERSION.$RELEASE-x86_64.iso"
curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=centreon&version=$VERSION&extension=iso&md5=$SRCHASH&ddos=1&dryrun=1"
