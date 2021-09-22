#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Fetch sources.
rm -rf "$PROJECT-server-$VERSION.tar.gz" "$PROJECT-server-$VERSION"
get_internal_source "map/$PROJECT-$VERSION-$RELEASE/$PROJECT-server-$VERSION.tar.gz"
tar xzf "$PROJECT-server-$VERSION.tar.gz"

# Tweak pom files.
cd "$PROJECT-server-$VERSION"
sed -i 's/<project.release>1/<project.release>'"$RELEASE"'/g' com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/pom.xml
sed -i 's/<project.release>1/<project.release>'"$RELEASE"'/g' com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/pom.xml

# Build with Maven.
mvn -q -f com.centreon.studio.server.parent/pom.xml clean install

# This is run once the Maven build terminated.
cd ..
FILES_TOMCAT6="$PROJECT-server-$VERSION/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/target/rpm/centreon-map4-server/RPMS/noarch/"'*.rpm'
FILES_TOMCAT7="$PROJECT-server-$VERSION/com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/target/rpm/centreon-map4-server/RPMS/noarch/"'*.rpm'
put_internal_rpms "3.4" "el7" "noarch" "map" "$PROJECT-$VERSION-$RELEASE" $FILES_TOMCAT7
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO rpm --resign "/srv/yum/internal/3.4/el7/noarch/map/$PROJECT-$VERSION-$RELEASE/*.rpm"
$SSH_REPO createrepo "/srv/yum/internal/3.4/el7/noarch/map/$PROJECT-$VERSION-$RELEASE"
