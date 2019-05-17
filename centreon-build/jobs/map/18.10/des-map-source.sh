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
cd "$PROJECT/web"
VERSIONWEB=`grep mod_release app/module/conf.php | cut -d '"' -f 4`
export VERSIONWEB="$VERSIONWEB"
VERSION=`echo "$VERSIONWEB" | cut -d . -f 1-2`
export VERSION="$VERSION"

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$COMMIT"

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Generate sources of Centreon Map web client.
npm ci

node ./node_modules/gulp/bin/gulp.js build-module
node ./node_modules/gulp/bin/gulp.js build-widget

# Generate Centreon Map web client source tarball used for packaging.
WEBDIR="../../centreon-map-web-client-$VERSIONWEB"
rm -rf "$WEBDIR"
mkdir "$WEBDIR"
cp -a build/module "$WEBDIR"
cp -a build/widget "$WEBDIR"
cp -a build/install.sh "$WEBDIR"
cp -a build/libinstall "$WEBDIR"
cp -a build/examples "$WEBDIR"
cd ../..
tar czf centreon-map-web-client-$VERSIONWEB.tar.gz centreon-map-web-client-$VERSIONWEB

# Generate Centreon Map client source tarball.
rm -rf "$PROJECT-desktop-$VERSION" "$PROJECT-desktop-$VERSION.tar.gz"
cp -r "$PROJECT/desktop" "$PROJECT-desktop-$VERSION"
tar czf "$PROJECT-desktop-$VERSION.tar.gz" "$PROJECT-desktop-$VERSION"

# Generate Centreon Map server source tarball.
rm -rf "$PROJECT-server-$VERSION" "$PROJECT-server-$VERSION.tar.gz"
cp -r "$PROJECT/server" "$PROJECT-server-$VERSION"
tar czf "$PROJECT-server-$VERSION.tar.gz" "$PROJECT-server-$VERSION"

# Send it to srvi-repo.
curl -F "file=@centreon-map-web-client-$VERSIONWEB.tar.gz" -F "version=71" 'http://encode.int.centreon.com/api/index.php' -o "centreon-map-web-client-$VERSIONWEB-php71.tar.gz"
put_internal_source "map" "$PROJECT-$VERSION-$RELEASE" "centreon-map-web-client-$VERSIONWEB.tar.gz"
put_internal_source "map" "$PROJECT-$VERSION-$RELEASE" "centreon-map-web-client-$VERSIONWEB-php71.tar.gz"
put_internal_source "map" "$PROJECT-$VERSION-$RELEASE" "$PROJECT/web/packaging/centreon-map-web-client.spectemplate"
put_internal_source "map" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-desktop-$VERSION.tar.gz"
put_internal_source "map" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-server-$VERSION.tar.gz"
put_internal_source "map" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
VERSIONWEB=$VERSIONWEB
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF

# Generate summary report.
rm -rf summary
cp -r `dirname $0`/../../common/build-artifacts summary
cp `dirname $0`/jobData.json summary/
sed -i -e "s/@VERSIONWEB@/$VERSIONWEB/g" summary/jobData.json
generate_summary
