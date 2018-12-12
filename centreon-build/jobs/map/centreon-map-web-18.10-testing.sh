#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-map-web-client

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Get version.
cd web
git checkout --detach "$COMMIT"
export VERSION=`grep mod_release app/module/conf.php | cut -d '"' -f 4`

# Generate sources of Centreon Map web client.
npm ci
./node_modules/bower/bin/bower install
node ./node_modules/gulp/bin/gulp.js build-module
node ./node_modules/gulp/bin/gulp.js build-widget

# Generate source tarball used for packaging.
rm -rf ../centreon-map-web-client-$VERSION
mkdir ../centreon-map-web-client-$VERSION
cp -a build/module ../centreon-map-web-client-$VERSION
cp -a build/widget ../centreon-map-web-client-$VERSION
cp -a build/install.sh ../centreon-map-web-client-$VERSION
cp -a build/libinstall ../centreon-map-web-client-$VERSION
cp -a build/examples ../centreon-map-web-client-$VERSION
cd ..
tar czf centreon-map-web-client-$VERSION.tar.gz centreon-map-web-client-$VERSION

# Encode source tarball.
curl -F "file=@centreon-map-web-client-$VERSION.tar.gz" -F "version=71" 'http://encode.int.centreon.com/api/index.php' -o "input/centreon-map-web-client-$VERSION-php71.tar.gz"

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies-18.10:centos7

# Build RPMs.
cp web/packaging/centreon-map-web-client.spectemplate input
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-18.10:centos7 input output-centos7

# Copy files to server.
put_testing_rpms "map" "18.10" "el7" "noarch" "map-web" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm

# Generate doc on internal server.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-map-4 -V master ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-map-4 -V master'"
