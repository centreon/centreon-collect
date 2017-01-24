#!/bin/sh

set -e
set -x

#Creating CBIS tarball

# Get version
VERSION=$(grep -m1 "<version>" $WORKSPACE/centreon-bi-engine/com.merethis.bi.cbis/pom.xml | awk -F[\>\<] {'print $3'})
export VERSION="$VERSION"

PRODUCT_NAME="centreon-bi-engine" 
PRODUCT_NAME_FULL="$PRODUCT_NAME-$VERSION-$RELEASE"

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

## Clone centreon-bi-report
cd ..
git clone https://centreon-bot:518bc6ce608956da1eadbe71ff7de731474b773b@github.com/centreon/centreon-bi-report.git
cd centreon-bi-report
git checkout $REPORTTAG 1>&2

rm -rf RPM-SPECS

cd ../


# Clone ETL project
git clone https://centreon-bot:518bc6ce608956da1eadbe71ff7de731474b773b@github.com/centreon/centreon-bi-etl.git
cd centreon-bi-etl
git checkout $ETLTAG 1>&2

rm -rf RPM-SPECS

cd ../

# Clone centreon-bi-reporting-server
git clone https://centreon-bot:518bc6ce608956da1eadbe71ff7de731474b773b@github.com/centreon/centreon-bi-reporting-server-light.git
cd centreon-bi-reporting-server-light
git checkout $LIGHTVERSION 1>&2

mv * ../
cd ../
rm -Rf centreon-bi-reporting-server-light

#Clean old tarball
rm -rf Centreon-MBI-Reporting-Server-Light*

#create tarball
mkdir Centreon-MBI-Reporting-Server-Light
mv centreon-bi* Centreon-MBI-Reporting-Server-Light/
mv install.sh Centreon-MBI-Reporting-Server-Light/
mv README-FIRST Centreon-MBI-Reporting-Server-Light/
mv silent-install.cfg Centreon-MBI-Reporting-Server-Light/

tar czf Centreon-MBI-Reporting-Server-Light-$renameVersion.tar.gz Centreon-MBI-Reporting-Server-Light

scp -o StrictHostKeyChecking=no Centreon-MBI-Reporting-Server-Light-$renameVersion.tar.gz "ubuntu@srvi-repo.int.centreon.com:/srv/sources/mbi/$REPO/"
