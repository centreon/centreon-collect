#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-bi-engine

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get version.
cd "$PROJECT"
VERSION=`grep cbis.version com.merethis.bi.cbis/pom.xml | cut -d '<' -f 2 | cut -d '>' -f 2`
export VERSION="$VERSION"

# Create source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../input/$PROJECT-$VERSION.tar.gz"
cp "packaging/$PROJECT.spectemplate" "../input/"
cd ..

# Pull latest build dependencies.
BUILD_IMG="ci.int.centreon.com:5000/mon-build-dependencies-18.10:centos7"
docker pull "$BUILD_IMG"

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG" input output

# Copy files to server.
put_testing_rpms "mbi" "18.10" "el7" "noarch" "$PROJECT" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
