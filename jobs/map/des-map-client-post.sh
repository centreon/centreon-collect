#!/bin/sh

set -e
set -x

#
# This is run once the Maven build terminated.
#

# Copy RPMs.
# scp -o StrictHostKeyChecking=no centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm ubuntu@srvi-repo.int.centreon.com:/srv/yum/internal/el6/noarch/RPMS/
# scp -o StrictHostKeyChecking=no centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm ubuntu@srvi-repo.int.centreon.com:/srv/yum/internal/el7/noarch/RPMS/
# DESTFILE=`ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mktemp`
# scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "ubuntu@srvi-repo.int.centreon.com:$DESTFILE"
# ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" sh $DESTFILE 'internal/el6/noarch'
# DESTFILE=`ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mktemp`
# scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "ubuntu@srvi-repo.int.centreon.com:$DESTFILE"
# ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" sh $DESTFILE 'internal/el7/noarch'

# Find version.
filename='centreon-studio-desktop-client/com.centreon.studio.client.packaging.deb.amd64/target/*.deb'
version=$(echo $filename | grep -Po '(?<=-client-)[0-9.]+')
major=`echo $version | cut -d . -f 1`
minor=`echo $version | cut -d . -f 2`
if [ -z "$major" -o -z "$minor" ] ; then
  echo 'COULD NOT DETECT VERSION, ABORTING.'
  exit 1
fi

# Recreate p2 directory.
path="/srv/p2/$major/$minor/"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" rm -rf $path
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p $path

# Copy modules.
scp -o StrictHostKeyChecking=no -r centreon-studio-desktop-client/com.centreon.studio.client.product/target/repository/* "ubuntu@srvi-repo.int.centreon.com":$path
