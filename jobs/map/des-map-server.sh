#!/bin/sh

set -e
set -x

# This is run once the Maven build terminated.
scp -o StrictHostKeyChecking=no centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm ubuntu@srvi-repo.int.centreon.com:/srv/yum/internal/el6/noarch/RPMS/
scp -o StrictHostKeyChecking=no centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm ubuntu@srvi-repo.int.centreon.com:/srv/yum/internal/el7/noarch/RPMS/
