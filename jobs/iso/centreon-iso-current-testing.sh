#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

##
## CENTOS 6
##

# Launch container.
ISO_IMAGE=ci.int.centreon.com:5000/mon-build-iso:centos6
docker pull $ISO_IMAGE
containerid=`docker create --privileged $ISO_IMAGE /usr/local/bin/container.sh $VERSION`

# Copy construction scripts to container.
docker cp `dirname $0`/../../packaging/iso "$containerid:/tmp/iso"
docker start -a "$containerid"
docker cp "$containerid:/tmp/build/centreon-standard-$VERSION-x86_64.iso" "ces.iso"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Send to srvi-repo.
scp -o StrictHostKeyChecking=no ces.iso "ubuntu@srvi-repo.int.centreon.com:/srv/iso/centreon-$VERSION.$RELEASE.el6.x86_64.iso"

##
## CENTOS 7
##

# Launch container.
ISO_IMAGE=ci.int.centreon.com:5000/mon-build-iso:centos7
docker pull $ISO_IMAGE
containerid=`docker create --privileged $ISO_IMAGE /usr/local/bin/container.sh $VERSION`

# Copy construction scripts to container.
docker cp `dirname $0`/../../containers/iso/centos7/addon "$containerid:/tmp/addon"
docker start -a "$containerid"
docker cp "$containerid:/tmp/ces.iso" "ces.iso"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Send to srvi-repo.
scp -o StrictHostKeyChecking=no ces.iso "ubuntu@srvi-repo.int.centreon.com:/srv/iso/centreon-$VERSION.$RELEASE.el7.x86_64.iso"
