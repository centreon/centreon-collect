#!/bin/sh

set -e
set -x

SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'

# This is run once the Maven build terminated.

# Copy files to internal repository.
TOMCAT6_PATH="centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/target/rpm/centreon-map4-server/RPMS/noarch/"
TOMCAT7_PATH="centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/target/rpm/centreon-map4-server/RPMS/noarch/"

for filename in "$TOMCAT6_PATH"/*.rpm; do
    TOMCAT6_FILE=$filename
done
for filename in "$TOMCAT7_PATH"/*.rpm; do
    TOMCAT7_FILE=$filename
done

scp -o StrictHostKeyChecking=no $TOMCAT6_PATH/$TOMCAT6_FILE ubuntu@srvi-repo.int.centreon.com:/srv/yum/map/3.4/el6/testing/noarch/RPMS/
scp -o StrictHostKeyChecking=no $TOMCAT7_PATH/$TOMCAT7_FILE ubuntu@srvi-repo.int.centreon.com:/srv/yum/map/3.4/el7/testing/noarch/RPMS/

# Sign RPMs
$SSH_REPO rpm --resign /srv/yum/map/3.4/el6/testing/noarch/RPMS/$TOMCAT6_FILE
$SSH_REPO rpm --resign /srv/yum/map/3.4/el7/testing/noarch/RPMS/$TOMCAT7_FILE

# update yum metadata
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/3.4/el6/testing/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/3.4/el7/testing/noarch

# Generate doc on internal server.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-map-4 -V master -p'"
