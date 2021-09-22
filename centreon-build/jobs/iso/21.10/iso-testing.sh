#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$RELEASE" ] ; then
  echo "You need to specify RELEASE environment variable."
  exit 1
fi
export VERSION=21.10

##
## CENTOS 7
##

# Launch container.
ISO_IMAGE=registry.centreon.com/mon-build-iso:centos7
docker pull $ISO_IMAGE
containerid=`docker create --privileged $ISO_IMAGE /usr/local/bin/container.sh 21.10`

# Copy construction scripts to container.
docker cp `dirname $0`"/../../../containers/iso/centos7/addon/21.10" "$containerid:/tmp/addon"
docker start -a "$containerid"
docker cp "$containerid:/tmp/ces.iso" "ces.iso"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Send to srvi-repo.
scp -o StrictHostKeyChecking=no ces.iso "ubuntu@srvi-repo.int.centreon.com:/srv/iso/centreon-$VERSION-$RELEASE.el7.x86_64.iso"
