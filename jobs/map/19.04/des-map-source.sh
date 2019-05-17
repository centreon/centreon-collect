#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

#
# This script will generate sources from the local clone of the project
# repository. These sources will then be pushed to the internal
# repository (srvi-repo) and used in downstream jobs, thanks to the
# property file generated at the end of the script.
#

# Project.
PROJECT=centreon-map
tar czf "$PROJECT-git.tar.gz" "$PROJECT"

# Get version.
cd "$PROJECT/server"
VERSIONSERVER=`grep '<version>' map-server-parent/map-server-packaging/map-server-packaging-tomcat7/pom.xml | cut -d '>' -f 2 | cut -d - -f 1`
export VERSIONSERVER="$VERSIONSERVER"
cd ../web
VERSIONWEB=`grep '$release = ' app/module/conf.php | cut -d "'" -f 2`
export VERSIONWEB="$VERSIONWEB"
VERSION=`echo "$VERSIONWEB" | cut -d . -f 1-2`
export VERSION="$VERSION"

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
if [ "$BUILD" '=' 'RELEASE' ] ; then
  export RELEASE="$BUILD_NUMBER"
else
  export RELEASE="$now.$COMMIT"
fi

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Installation of the dependency packages verified by the "package-lock.json" file
# present in the web repository of centreon-map, which therefore allows to obtain
# the "node_modules".
npm ci

# Generate sources of Centreon Map web client.
# Launch this command which allows you to build the module and widget in a "build" folder.
npm run build

# Generate Centreon Map web client source tarball used for packaging.
WEBDIR="../../$PROJECT-web-client-$VERSIONWEB"
rm -rf "$WEBDIR"
mkdir "$WEBDIR"
cp -a build/module "$WEBDIR"
cp -a build/widget "$WEBDIR"
cp -a build/install.sh "$WEBDIR"
cp -a build/libinstall "$WEBDIR"
cp -a build/examples "$WEBDIR"
cd ../..
tar czf $PROJECT-web-client-$VERSIONWEB.tar.gz $PROJECT-web-client-$VERSIONWEB

# Generate Centreon Map client source tarball.
rm -rf "$PROJECT-desktop-$VERSION" "$PROJECT-desktop-$VERSION.tar.gz"
cp -r "$PROJECT/desktop" "$PROJECT-desktop-$VERSION"
tar czf "$PROJECT-desktop-$VERSION.tar.gz" "$PROJECT-desktop-$VERSION"

# Generate Centreon Map server source tarball.
rm -rf "$PROJECT-server-$VERSIONSERVER" "$PROJECT-server-$VERSIONSERVER.tar.gz"
cp -r "$PROJECT/server" "$PROJECT-server-$VERSIONSERVER"
sed -i 's/<project.release>1/<project.release>'"$RELEASE"'/g' "$PROJECT-server-$VERSIONSERVER/map-server-parent/map-server-packaging/map-server-packaging-tomcat7/pom.xml"
tar czf "$PROJECT-server-$VERSIONSERVER.tar.gz" "$PROJECT-server-$VERSIONSERVER"

# Send it to srvi-repo.
curl -F "file=@$PROJECT-web-client-$VERSIONWEB.tar.gz" -F "version=71" 'http://encode.int.centreon.com/api/index.php' -o "$PROJECT-web-client-$VERSIONWEB-php71.tar.gz"
put_internal_source "map" "$PROJECT-web-$VERSIONWEB-$RELEASE" "$PROJECT-web-client-$VERSIONWEB.tar.gz"
put_internal_source "map" "$PROJECT-web-$VERSIONWEB-$RELEASE" "$PROJECT-web-client-$VERSIONWEB-php71.tar.gz"
put_internal_source "map" "$PROJECT-web-$VERSIONWEB-$RELEASE" "$PROJECT/web/packaging/$PROJECT-web-client.spectemplate"
put_internal_source "map" "$PROJECT-desktop-$VERSION-$RELEASE" "$PROJECT-desktop-$VERSION.tar.gz"
put_internal_source "map" "$PROJECT-server-$VERSIONSERVER-$RELEASE" "$PROJECT-server-$VERSIONSERVER.tar.gz"
put_internal_source "map" "$PROJECT-server-$VERSIONSERVER-$RELEASE" "$PROJECT-git.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
VERSIONSERVER=$VERSIONSERVER
VERSIONWEB=$VERSIONWEB
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF

# Generate summary report.
rm -rf summary
cp -r `dirname $0`/../../common/build-artifacts summary
cp `dirname $0`/jobData.json summary/
sed -i -e "s/@VERSIONSERVER@/$VERSIONSERVER/g" -e "s/@VERSIONWEB@/$VERSIONWEB/g" summary/jobData.json
generate_summary
