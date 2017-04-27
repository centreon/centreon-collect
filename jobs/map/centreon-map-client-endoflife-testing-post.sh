#!/bin/sh

set -e
set -x

#
# This is run once the Maven build terminated.
#

# Find version.
filename='centreon-studio-desktop-client/com.centreon.studio.client.packaging.deb.amd64/target/*.deb'
VERSION=$(echo $filename | grep -Po '(?<=-client-)[0-9.]+')
major=`echo $VERSION | cut -d . -f 1`
minor=`echo $VERSION | cut -d . -f 2`
patch=`echo $VERSION | cut -d . -f 3`
if [ -z "$major" -o -z "$minor" -o -z "$patch" ] ; then
  echo 'COULD NOT DETECT VERSION, ABORTING.'
  exit 1
fi

# Recreate p2 directory.
path="/srv/p2/testing/$major/$minor/"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" rm -rf $path
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p $path

# Copy modules.
scp -o StrictHostKeyChecking=no -r centreon-studio-desktop-client/com.centreon.studio.client.product/target/repository/* "ubuntu@srvi-repo.int.centreon.com":$path

# Move all installers to an install folder.
rm -rf installs
mkdir installs
cp centreon-studio-desktop-client/com.centreon.studio.client.packaging.deb.amd64/target/*.deb installs/
cp centreon-studio-desktop-client/com.centreon.studio.client.packaging.nsis.x86_64/target/*.exe installs/
cp centreon-studio-desktop-client/com.centreon.studio.client.product/target/products/Centreon-Map4.product-macosx.cocoa.x86_64.tar.gz installs/

# Copy installers to remote repository.
path="/srv/sources/map/testing/centreon-map-client-$VERSION"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" rm -rf "$path"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "$path"
scp -o StrictHostKeyChecking=no -r installs/* "ubuntu@srvi-repo.int.centreon.com:$path"
