#!/bin/sh

set -e
set -x

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
tar czf input/centreon-map4-web-client-$VERSION.tar.gz centreon-map4-web-client-$VERSION

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies:centos7

# Build RPMs.
cp web/packaging/centreon-map4-web-client.spectemplate input

docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies:centos7 input output-centos7


# Copy files to server.
FILES_CENTOS7='output-centos7/noarch/*.rpm'

REPO='testing'
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/map/3.4/el7/$REPO/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/map/3.4/el7/$REPO/noarch
