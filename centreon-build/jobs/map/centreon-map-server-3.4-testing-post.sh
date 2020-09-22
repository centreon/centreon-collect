#!/bin/sh

set -e
set -x

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# This is run once the Maven build terminated.

# Copy the generated RPM files into the internal RPM repository.

# Get the folder path of the 2 generated RPMs (for CentOS 6 and CentOS 7).
TOMCAT6_PATH="server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/target/rpm/centreon-map4-server/RPMS/noarch/"
TOMCAT7_PATH="server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/target/rpm/centreon-map4-server/RPMS/noarch/"

# Get the name of each RPM files.
# This way of getting the RPM's names is not pretty but it works.
for filename in "$TOMCAT6_PATH"/*.rpm; do
    TOMCAT6_FILE=${filename##*/}
done
for filename in "$TOMCAT7_PATH"/*.rpm; do
    TOMCAT7_FILE=${filename##*/}
done

# Copy the RPMs into the remote internal RPM repository.
scp -o StrictHostKeyChecking=no $TOMCAT6_PATH/$TOMCAT6_FILE ubuntu@srvi-repo.int.centreon.com:/srv/yum/map/3.4/el6/testing/noarch/RPMS/
scp -o StrictHostKeyChecking=no $TOMCAT7_PATH/$TOMCAT7_FILE ubuntu@srvi-repo.int.centreon.com:/srv/yum/map/3.4/el7/testing/noarch/RPMS/

# Sign RPMs on directly on the remote internal RPM repository.
$SSH_REPO rpm --resign /srv/yum/map/3.4/el6/testing/noarch/RPMS/$TOMCAT6_FILE
$SSH_REPO rpm --resign /srv/yum/map/3.4/el7/testing/noarch/RPMS/$TOMCAT7_FILE

# update yum metadata so that the remote RPM repository will be up to date
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/3.4/el6/testing/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/3.4/el7/testing/noarch
