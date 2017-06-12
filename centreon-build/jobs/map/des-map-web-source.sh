#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

#
# This script will generate sources from the local clone of the project
# repository. These sources will then be pushed to the internal
# repository (srvi-repo) and used in downstream jobs, thanks to the
# property file generated at the end of the script.
#

# Project.
PROJECT=centreon-map-web

# Get version.
cd "centreon-studio-web-client"
VERSION=`grep mod_release app/module/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$COMMIT"

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Generate sources of Centreon Map web client.
npm install
./node_modules/bower/bin/bower install
node ./node_modules/gulp/bin/gulp.js build-module
node ./node_modules/gulp/bin/gulp.js build-widget

# Generate source tarball used for packaging.
rm -rf ../centreon-map4-web-client-$VERSION
mkdir ../centreon-map4-web-client-$VERSION
cp -a build/module ../centreon-map4-web-client-$VERSION
cp -a build/widget ../centreon-map4-web-client-$VERSION
cp -a build/install.sh ../centreon-map4-web-client-$VERSION
cp -a build/libinstall ../centreon-map4-web-client-$VERSION
cp -a build/examples ../centreon-map4-web-client-$VERSION
cd ..
tar czf centreon-map4-web-client-$VERSION.tar.gz centreon-map4-web-client-$VERSION

# Send it to srvi-repo.
put_internal_source "map-web" "centreon-map4-web-client-$VERSION-$RELEASE" "centreon-map4-web-client-$VERSION.tar.gz"
put_internal_source "map-web" "centreon-map4-web-client-$VERSION-$RELEASE" "centreon-studio-web-client/packaging/centreon-map4-web-client.spectemplate"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
