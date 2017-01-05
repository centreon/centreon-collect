#!/bin/sh

set -e
set -x

#
# This is run before the Maven build.
#

# Get release.
cd centreon-studio-server
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
RELEASE="$now.$commit"

# Tweak pom files.
sed -i 's/<project.release>1/<project.release>'"$RELEASE"'/g' com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat6/pom.xml
sed -i 's/<project.release>1/<project.release>'"$RELEASE"'/g' com.centreon.studio.server.parent/com.centreon.studio.map.server/com.centreon.studio.map.server.packaging/com.centreon.studio.map.server.packaging.tomcat7/pom.xml
