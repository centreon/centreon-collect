#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-pp-manager

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
BUILD_CENTOS7=ci.int.centreon.com:5000/mon-build-dependencies-18.10:centos7
docker pull "$BUILD_CENTOS7"

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Get version.
cd "$PROJECT"
git checkout --detach "$COMMIT"
VERSION=`grep mod_release www/modules/$PROJECT/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Create source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
cd ..

# Encrypt source tarball.
curl -F "file=@$PROJECT-$VERSION.tar.gz" -F 'version=71' -F "modulename=$PROJECT" 'http://encode.int.centreon.com/api/' -o "input/$PROJECT-$VERSION-php71.tar.gz"

# Copy spectemplate.
cp "$PROJECT/packaging/$PROJECT.spectemplate" input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input output-centos7

# Copy files to server.
put_testing_source "standard" "ppm" "$PROJECT-$VERSION-$RELEASE" "input/$PROJECT-$VERSION-php71.tar.gz"
put_testing_rpms "standard" "18.10" "el7" "noarch" "ppm" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
