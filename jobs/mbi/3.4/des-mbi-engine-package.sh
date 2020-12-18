#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7>"
  exit 1
fi
DISTRIB="$1"
PROJECT=centreon-bi-engine
# Get version
VERSION=$(grep -m1 "<version>" $WORKSPACE/$PROJECT/com.merethis.bi.cbis/pom.xml | awk -F[\>\<] {'print $3'})
export VERSION="$VERSION"

PRODUCT_NAME="centreon-bi-engine"
PRODUCT_NAME_FULL="$PRODUCT_NAME-$VERSION$release"
SPECS_NAME="$PRODUCT_NAME_FULL.spec"

# Get release
cd $WORKSPACE/centreon-bi-engine
commit=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

ARCHIVE_NAME="$PRODUCT_NAME_FULL-$RELEASE.tar.gz"

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
 rm -rf output
 mkdir input
 mkdir output

 # Move tarball and spec to build into input/ folder
 mv $WORKSPACE/$ARCHIVE_NAME input/
 mv $SPECS_NAME input/

# Pull latest build dependencies.
BUILD_IMG="registry.centreon.com/mon-build-dependencies-3.4:$DISTRIB"
docker pull "$BUILD_IMG"

# Build RPMs.
#sed 's/@/@@/g' < centreon-bi-etl/RPM-SPECS/centreon-bi-etl.spec > input/centreon-bi-etl.spectemplate
#sed -i 's/^Release:.*$/Release: '"$RELEASE"'%{?dist}/g' input/centreon-bi-etl.spectemplate
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_IMG" input output

# Copy files to server.
DISTRIB='el7'
put_internal_rpms "3.4" "$DISTRIB" "noarch" "mbi-engine" "$PROJECT-$VERSION-$RELEASE" output/noarch/*.rpm
