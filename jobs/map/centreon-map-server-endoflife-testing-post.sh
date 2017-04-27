#!/bin/sh

set -e
set -x

# This is run once the Maven build terminated.

# Copy files to internal repository.
FILE_CENTOS6=centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm
FILE_CENTOS7=centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm
scp -o StrictHostKeyChecking=no $FILE_CENTOS6 ubuntu@srvi-repo.int.centreon.com:/srv/yum/map/3.4/el6/testing/noarch/RPMS/
scp -o StrictHostKeyChecking=no $FILE_CENTOS7 ubuntu@srvi-repo.int.centreon.com:/srv/yum/map/3.4/el7/testing/noarch/RPMS/
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/3.4/el6/testing/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/3.4/el7/testing/noarch

# Generate doc on internal server.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-map-4 -V master -p'"
