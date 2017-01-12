#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Get version
VERSION=$(grep -m1 "<version>" $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/pom.xml | awk -F[\>\<] {'print $3'})
export VERSION="$VERSION"

PRODUCT_NAME="centreon-bi-engine" 
PRODUCT_NAME_FULL="$PRODUCT_NAME-$VERSION$release"
SPECS_NAME="$PRODUCT_NAME_FULL.spec"

# Get release
cd $WORKSPACE/centreon-bi-engine
commit=`git log -1 "1476e1df2a10a12de05b2a44572acc6ca1d3fbcb" --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

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

 # Create input and output files for Docker to build
 rm -rf input
 rm -rf output
 mkdir input
 mkdir output

 # Move tarball and spec to build into input/ folder
 mv $WORKSPACE/$ARCHIVE_NAME input/
 mv $SPECS_NAME input/
 
# Pull latest build dependencies.
BUILD_IMG="ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
#sed 's/@/@@/g' < centreon-bi-etl/RPM-SPECS/centreon-bi-etl.spec > input/centreon-bi-etl.spectemplate
#sed -i 's/^Release:.*$/Release: '"$RELEASE"'%{?dist}/g' input/centreon-bi-etl.spectemplate
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_IMG" input output

# Copy files to server.
if [ "$DISTRIB" = 'centos6' ] ; then
  REPO='internal/el6/noarch'
elif [ "$DISTRIB" = 'centos7' ] ; then
  REPO='internal/el7/noarch'
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
FILES='output/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "ubuntu@srvi-repo.int.centreon.com:/srv/yum/$REPO/RPMS"
DESTFILE=`ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mktemp`
scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "ubuntu@srvi-repo.int.centreon.com:$DESTFILE"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" sh $DESTFILE $REPO
