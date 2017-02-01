#!/bin/sh

set -e
set -x

# Get version
VERSION=$(grep -m1 "<version>" $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/pom.xml | awk -F[\>\<] {'print $3'})
export VERSION="$VERSION"

PRODUCT_NAME="centreon-bi-engine"
PRODUCT_NAME_FULL="$PRODUCT_NAME-$VERSION-$RELEASE"
SPECS_NAME="$PRODUCT_NAME_FULL.spec"

ARCHIVE_NAME="$PRODUCT_NAME_FULL-$RELEASE.tar.gz"

# Clean workspace
rm -rf $WORKSPACE/build/

# Create build folders
mkdir -p $WORKSPACE/build/bin/external_lib/

# Build BIRT library
BIRT_ZIP_NAME="birt-runtime-osgi-4_4_2-20150217"
BIRT_NAME="birt-runtime-osgi-4_4_2"

# Extract BIRT report engine
rm -rf $BIRT_NAME
wget http://srvi-repo.int.centreon.com/sources/mbi/stable/$BIRT_ZIP_NAME.zip
unzip $BIRT_ZIP_NAME.zip
cp -R $BIRT_NAME/ReportEngine/ $WORKSPACE/build/

cd $WORKSPACE

# Copy maven built files
cp -R $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/deploy/* $WORKSPACE/build/
cp -R $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/com.merethis.bi.cbis.engine/target/*.jar $WORKSPACE/build/bin/cbis.jar
cp -R $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/com.merethis.bi.cbis.engine/target/cbis_lib/*.jar $WORKSPACE/build/bin/

# Replace MySQL connector with MariaDB connector
rm -rf $WORKSPACE/build/bin/cbis_lib/mysql-connector*

# Add MariaDB connector to BIRT installation
cp $WORKSPACE/build/bin/mariadb-java-client-*.jar $WORKSPACE/build/ReportEngine/plugins/org.eclipse.birt.report.data.oda.jdbc_*/drivers/


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

# Create input and output directories for Docker build
rm -rf input
rm -rf output-centos6
rm -rf output-centos7
mkdir input
mkdir output-centos6
mkdir output-centos7

# Move tarball and spec to build into input/ folder
mv $WORKSPACE/$ARCHIVE_NAME input/
mv $SPECS_NAME input/

# Pull latest build dependencies.
BUILD_IMG_CENTOS_6="ci.int.centreon.com:5000/mon-build-dependencies:centos6"
BUILD_IMG_CENTOS_7="ci.int.centreon.com:5000/mon-build-dependencies:centos7"

docker pull "$BUILD_IMG_CENTOS_6"
docker pull "$BUILD_IMG_CENTOS_7"

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS_6" input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG_CENTOS_7" input output-centos7

# Copy files to server.
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'

scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/mbi/3.4/el6/$REPO/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/mbi/3.4/el7/$REPO/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/mbi/3.4/el6/$REPO/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/mbi/3.4/el7/$REPO/noarch
