#!/bin/sh

set -e
set -x

# Project.
export PROJECT=centreon-iso
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# Check arguments.
if [ -z "$RELEASE" ] ; then
  echo "You need to specify RELEASE environment variable."
  exit 1
fi
export VERSION=3.4

# Release ISO for el7
SRCHASH=`$SSH_REPO "md5sum /srv/iso/centreon-$VERSION.$RELEASE.el7.x86_64.iso | cut -d ' ' -f 1"`
$SSH_REPO aws s3 cp --acl public-read "/srv/iso/centreon-$VERSION.$RELEASE.el7.x86_64.iso" "s3://centreon-iso/stable/centreon-$VERSION.$RELEASE.el7.x86_64.iso"

# Sync ISO in database (dryrun=1 does not show ISO on website)
OUTPUT=`curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=centreon&version=$VERSION.$RELEASE.el7.x86_64&extension=iso&md5=$SRCHASH&ddos=1&dryrun=1"`
SUCCESS=`echo $OUTPUT | python -c 'import json,sys;obj=json.load(sys.stdin);print obj["status"]'`
if [ \( "$SUCCESS" '!=' "success" \) ] ; then
  echo "ISO synchronization failed."
  exit 1
fi

DOWNLOADID=`echo $OUTPUT | python -c 'import json,sys;obj=json.load(sys.stdin);print obj["id"]'`

# enable download link on website
curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&action=enable&release_id=$DOWNLOADID"

# Check if download link is available
STATUSCODE=`curl -s -w "%{http_code}" "https://download.centreon.com/?action=download&id=$DOWNLOADID" -o /dev/null`
if [ \( "$STATUSCODE" -ne "200" \) ] ; then
  echo "ISO cannot be downloaded."
  exit 1
fi
