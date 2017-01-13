#!/bin/sh

set -e
set -x

# This is run once the Maven build terminated.

FILE_CENTOS6=centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm
FILE_CENTOS7=centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm

scp -o StrictHostKeyChecking=no $FILE_CENTOS6 ubuntu@srvi-repo.int.centreon.com:/srv/yum/map/bfcfef6922ae08bd2b641324188d8a5f/3.4/el6/$REPO/noarch/RPMS/
scp -o StrictHostKeyChecking=no $FILE_CENTOS7 ubuntu@srvi-repo.int.centreon.com:/srv/yum/map/bfcfef6922ae08bd2b641324188d8a5f/3.4/el7/$REPO/noarch/RPMS/

ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/bfcfef6922ae08bd2b641324188d8a5f/3.4/el6/$REPO/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/bfcfef6922ae08bd2b641324188d8a5f/3.4/el7/$REPO/noarch
