#!/bin/sh

set -e
set -x

# Get version
VERSION=$(grep -m1 "<version>" $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/pom.xml | awk -F[\>\<] {'print $3'})
export VERSION="$VERSION"

PRODUCT_NAME="centreon-bi-engine"
PRODUCT_NAME_FULL="$PRODUCT_NAME-$VERSION-$RELEASE"
SPECS_NAME="$PRODUCT_NAME_FULL.spec"

# Get release
cd $WORKSPACE/centreon-bi-engine
ARCHIVE_NAME="$PRODUCT_NAME_FULL.tar.gz"

# Clean workspace
rm -rf $WORKSPACE/build/

# Create build folders
mkdir -p $WORKSPACE/build/bin/external_lib/

cd $WORKSPACE

# Copy maven built files
cp -R $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/deploy/* $WORKSPACE/build/
cp -R $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/com.merethis.bi.cbis.engine/target/*.jar $WORKSPACE/build/bin/cbis.jar
mkdir $WORKSPACE/build/bin/cbis_lib/
cp -R $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/com.merethis.bi.cbis.engine/target/cbis_lib/*.jar $WORKSPACE/build/bin/cbis_lib/

# Copy all files into a correct name folder
mv $WORKSPACE/build $WORKSPACE/$PRODUCT_NAME-$VERSION/

# Create tarball
tar cfvz  $ARCHIVE_NAME $PRODUCT_NAME-$VERSION
# Clean archived folder
rm -rf $WORKSPACE/$PRODUCT_NAME-$VERSION

# Change spec file name
cp $WORKSPACE/centreon-bi-engine/RPM-SPECS/$PRODUCT_NAME.spec $SPECS_NAME

# Change spec version, release and source numbers
 sed -i -e  "s/^Release:.*/Release: $RELEASE%{?dist}/g" "$SPECS_NAME"
 sed -i -e  "s/^Source0:.*/Source0:%{name}-%{version}-$RELEASE.tar.gz/g" "$SPECS_NAME"

 # Create input and output files for Docker to build
 rm -rf input
 mkdir input

 rm -rf output6
 rm -rf output7

 mkdir output6
 mkdir output7

 # Move tarball and spec to build into input/ folder
 mv $WORKSPACE/$ARCHIVE_NAME input/
 mv $SPECS_NAME input/

# Pull latest build dependencies.
 BUILD_IMG_CENTOS7="registry.centreon.com/mon-build-dependencies:centos7"
 docker pull "$BUILD_IMG_CENTOS7"

docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS7" input output7

# Copy files to server.
REPO7='internal/el7/noarch'

FILES_CENTOS7='output7/noarch/*.rpm'

scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/mbi/3.4/el7/$REPO/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/mbi/3.4/el7/$REPO/noarch
