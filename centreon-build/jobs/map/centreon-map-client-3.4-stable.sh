#!/bin/sh

set -e
set -x

export PROJECT='centreon-map-client'

# Check arguments.
if [ -z "$VERSION" ] ; then
  echo "You need to specify VERSION environment variable."
  exit 1
fi

export MAJOR=`echo $VERSION | cut -d . -f 1`
export MINOR=`echo $VERSION | cut -d . -f 2`
export BUGFIX=`echo $VERSION | cut -d . -f 3`

# Move artifacts to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO rm -rf "/srv/sources/map/stable/$PROJECT-$VERSION/"
$SSH_REPO mkdir "/srv/sources/map/stable/$PROJECT-$VERSION/"
$SSH_REPO mv "/srv/sources/map/testing/$PROJECT-$VERSION/*" "/srv/sources/map/stable/$PROJECT-$VERSION/"

# Upload installers to centreon download website
# The url is specific for this version. We have to change it for new minor versions

BASE_INTERNAL_URL="/srv/sources/map/stable/$PROJECT-$VERSION/"
BASE_EXTERNAL_URL="s3://centreon-download/enterprises/centreon-map/centreon-map-$MAJOR.$MINOR/centreon-map-$MAJOR.$MINOR/9ae03a4457fa0ce578379a4e0c8b51f2/"

# Copy MacOS .tar.gz version
PRODUCT_NAME='Centreon-Map4.product-macosx.cocoa.x86_64.tar.gz'
$SSH_REPO aws s3 cp --acl public-read "$BASE_INTERNAL_URL/$PRODUCT_NAME" "$BASE_EXTERNAL_URL/$PRODUCT_NAME"

# Copy Windows .exe
PRODUCT_NAME="centreon-map4-desktop-client-$VERSION-SNAPSHOT-x86_64.exe"
EXTERNAL_PRODUCT_NAME="centreon-map4-desktop-client-$VERSION-x86_64.exe"
$SSH_REPO aws s3 cp --acl public-read "$BASE_INTERNAL_URL/$PRODUCT_NAME" "$BASE_EXTERNAL_URL/$EXTERNAL_PRODUCT_NAME"

# Copy Ubuntu .deb
PRODUCT_NAME="centreon-map4-desktop-client-$VERSION-SNAPSHOT-x86_64.deb"
EXTERNAL_PRODUCT_NAME="centreon-map4-desktop-client-$VERSION-x86_64.deb"
$SSH_REPO aws s3 cp --acl public-read "$BASE_INTERNAL_URL/$PRODUCT_NAME" "$BASE_EXTERNAL_URL/$EXTERNAL_PRODUCT_NAME"

# Copy p2 artifacts to remote server.
$SSH_REPO ssh -o StrictHostKeyChecking=no "map-repo@yum.int.centreon.com" rm -rf "centreon-studio-repository/$MAJOR/$MINOR"
$SSH_REPO scp -r "/srv/p2/testing/$MAJOR/$MINOR" "map-repo@yum.int.centreon.com:centreon-studio-repository/$MAJOR/$MINOR"
