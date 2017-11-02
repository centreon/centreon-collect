#!/bin/sh

set -e
set -x

#
# This is run before the Maven build.
#

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Checkout commit.
cd centreon-map/server
git checkout --detach "$COMMIT"

# Tweak pom files to add release version.
sed -i 's/<project.release>1/<project.release>'"$RELEASE"'/g' com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/pom.xml
sed -i 's/<project.release>1/<project.release>'"$RELEASE"'/g' com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/pom.xml
