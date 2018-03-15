#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Prepare environment
rm -rf centreon-build-containers
cp -r /opt/centreon-build/containers centreon-build-containers
cd centreon-build-containers

# Build images.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:debian9-armhf
docker build -t ci.int.centreon.com:5000/centreon-plugins:debian9-armhf -f plugins/debian.Dockerfile .
docker push ci.int.centreon.com:5000/centreon-plugins:debian9-armhf

docker pull centos:7
docker build -t ci.int.centreon.com:5000/centreon-plugins:centos7 -f plugins/centos.Dockerfile .
docker push ci.int.centreon.com:5000/centreon-plugins:centos7

# Build RPMs.
containerid=`docker create ci.int.centreon.com:5000/centreon-plugins:centos7`
docker start -a "$containerid"

# Publish RPMs.
rm -rf el6
rm -rf el7
docker cp "$containerid:/script/centreon-plugins/el6" .
docker cp "$containerid:/script/centreon-plugins/el7" .

for distrib in el6 el7 ; do
    FILES_LIST="$(ls $distrib/*.$distrib.*.rpm 2>/dev/null)"
    for file in $FILES_LIST; do
        scp $distrib/$file ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/$distrib/testing/noarch/RPMS/
    done
done

# Cleanup.
docker rm "$containerid"


# Build RPMs.
containerid=`docker create ci.int.centreon.com:5000/centreon-plugins:debian9-armhf`
docker start -a "$containerid"

# Publish RPMs.
rm -rf armhf
docker cp "$containerid:/script/centreon-plugins/armhf" .

put_internal_debs "3.5" "stretch" armhf/*.deb

# Cleanup.
docker rm "$containerid"