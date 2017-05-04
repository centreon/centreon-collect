#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Launch container.
ISO_IMAGE=ci.int.centreon.com:5000/mon-build-iso:latest
docker pull $ISO_IMAGE
containerid=`docker create $ISO_IMAGE /usr/local/bin/container.sh $VERSION`

# Copy construction scripts to container.
docker cp `dirname $0/../../packaging/iso /iso`
docker start -a "$containerid"
docker cp "$containerid:/build/centreon-standard-$VERSION-x86_64.iso" "ces.iso"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Send to srvi-repo.
scp -o StrictHostKeyChecking=no ces.iso "ubuntu@srvi-repo.int.centreon.com:/srv/iso/centreon-$VERSION.$RELEASE-el6-x86_64.iso"
