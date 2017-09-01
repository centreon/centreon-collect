#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Fetch sources.
rm -rf "$PROJECT-desktop-$VERSION.tar.gz" "$PROJECT-desktop-$VERSION"
get_internal_source "map/$PROJECT-$VERSION-$RELEASE/$PROJECT-desktop-$VERSION.tar.gz"
tar xzf "$PROJECT-desktop-$VERSION.tar.gz"

# Build with Maven.
cd "$PROJECT-desktop-$VERSION/desktop"
mvn -f com.centreon.studio.client.parent/pom.xml clean install

# Find version.
major=`echo $VERSION | cut -d . -f 1`
minor=`echo $VERSION | cut -d . -f 2`
if [ -z "$major" -o -z "$minor" ] ; then
  echo 'COULD NOT DETECT VERSION, ABORTING.'
  exit 1
fi

# Recreate p2 directory.
path="/srv/p2/unstable/$major/$minor/"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" rm -rf $path
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p $path
scp -o StrictHostKeyChecking=no -r centreon-studio-desktop-client/com.centreon.studio.client.product/target/repository/* "ubuntu@srvi-repo.int.centreon.com":$path

# Move all installers to an install folder.
rm -rf ../../installs
mkdir ../../installs
cp com.centreon.studio.client.packaging.deb.amd64/target/*.deb ../../installs/
cp com.centreon.studio.client.packaging.nsis.x86_64/target/*.exe ../../installs/
cp com.centreon.studio.client.product/target/products/Centreon-Map4.product-macosx.cocoa.x86_64.tar.gz ../../installs/
cd ../..

# Copy installers to remote repository
scp -o StrictHostKeyChecking=no -r installs/* "ubuntu@srvi-repo.int.centreon.com:/srv/p2/unstable/"
