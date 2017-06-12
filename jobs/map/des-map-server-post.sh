#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# This is run once the Maven build terminated.
FILES_TOMCAT6='centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm'
FILES_TOMCAT7='centreon-studio-server/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/target/rpm/centreon-map4-server/RPMS/noarch/*.rpm'
put_internal_rpms "3.4" "el6" "noarch" "map-server" "" $FILES_TOMCAT6
put_internal_rpms "3.4" "el7" "noarch" "map-server" "" $FILES_TOMCAT7
put_internal_rpms "3.5" "el6" "noarch" "map-server" "" $FILES_TOMCAT6
put_internal_rpms "3.5" "el7" "noarch" "map-server" "" $FILES_TOMCAT7
