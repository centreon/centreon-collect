#!/bin/sh

set -e
set -x

export PROJECT=centreon-map

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

export MAJOR=`echo $VERSION | cut -d . -f 1`
export MINOR=`echo $VERSION | cut -d . -f 2`
export BUGFIX=`echo $VERSION | cut -d . -f 3`

# Move artifacts to the stable directory.
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO rm -rf "/srv/sources/map/stable/$PROJECT-desktop-$MAJOR.$MINOR-$RELEASE"
$SSH_REPO mv "/srv/sources/map/testing/map-desktop/$PROJECT-desktop-$MAJOR.$MINOR-$RELEASE" "/srv/sources/map/stable/"

# Upload installers to centreon download website
# The url is specific for this version. We have to change it for new minor versions

BASE_INTERNAL_URL="/srv/sources/map/stable/$PROJECT-desktop-$MAJOR.$MINOR-$RELEASE/"
PRIVATE_EXTERNAL_URL="s3://centreon-download/enterprises/centreon-map/centreon-map-$MAJOR.$MINOR/centreon-map-$MAJOR.$MINOR/9ae03a4457fa0ce578379a4e0c8b51f2/"
PUBLIC_EXTERNAL_URL="s3://centreon-download/public/centreon-map"

# Copy MacOS .tar.gz version
PRODUCT_NAME='Centreon-Map4.product-macosx.cocoa.x86_64.tar.gz'
EXTERNAL_PRODUCT_NAME="centreon-map-$VERSION.product-macosx.cocoa.x86_64.tar.gz"
$SSH_REPO aws s3 cp --acl public-read "$BASE_INTERNAL_URL/$PRODUCT_NAME" "$PRIVATE_EXTERNAL_URL/$EXTERNAL_PRODUCT_NAME"
$SSH_REPO aws s3 cp --acl public-read "$BASE_INTERNAL_URL/$PRODUCT_NAME" "$PUBLIC_EXTERNAL_URL/$EXTERNAL_PRODUCT_NAME"
SRCHASH=`$SSH_REPO "md5sum $BASE_INTERNAL_URL/$PRODUCT_NAME | cut -d ' ' -f 1"`
curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION.product-macosx.cocoa.x86_64&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=0"

# Copy Windows .exe
PRODUCT_NAME="centreon-map4-desktop-client-$VERSION-SNAPSHOT-x86_64.exe"
EXTERNAL_PRODUCT_NAME="centreon-map-desktop-client-$VERSION-x86_64-windows.exe"
$SSH_REPO aws s3 cp --acl public-read "$BASE_INTERNAL_URL/$PRODUCT_NAME" "$PRIVATE_EXTERNAL_URL/$EXTERNAL_PRODUCT_NAME"
$SSH_REPO aws s3 cp --acl public-read "$BASE_INTERNAL_URL/$PRODUCT_NAME" "$PUBLIC_EXTERNAL_URL/$EXTERNAL_PRODUCT_NAME"
SRCHASH=`$SSH_REPO "md5sum $BASE_INTERNAL_URL/$PRODUCT_NAME | cut -d ' ' -f 1"`
curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=desktop-client-$VERSION-x86_64-windows&extension=exe&md5=$SRCHASH&ddos=0&dryrun=0"

# Copy Ubuntu .deb
PRODUCT_NAME="centreon-map4-desktop-client-$VERSION-SNAPSHOT-x86_64.deb"
EXTERNAL_PRODUCT_NAME="centreon-map-desktop-client-$VERSION-x86_64-debian.deb"
$SSH_REPO aws s3 cp --acl public-read "$BASE_INTERNAL_URL/$PRODUCT_NAME" "$PRIVATE_EXTERNAL_URL/$EXTERNAL_PRODUCT_NAME"
$SSH_REPO aws s3 cp --acl public-read "$BASE_INTERNAL_URL/$PRODUCT_NAME" "$PUBLIC_EXTERNAL_URL/$EXTERNAL_PRODUCT_NAME"
SRCHASH=`$SSH_REPO "md5sum $BASE_INTERNAL_URL/$PRODUCT_NAME | cut -d ' ' -f 1"`
curl "https://download.centreon.com/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=desktop-client-$VERSION-x86_64-debian&extension=deb&md5=$SRCHASH&ddos=0&dryrun=0"

# Copy p2 artifacts to remote server.
$SSH_REPO ssh -o StrictHostKeyChecking=no "map-repo@10.24.1.107" rm -rf "centreon-studio-repository/$MAJOR/$MINOR"
$SSH_REPO scp -r "/srv/p2/testing/$MAJOR/$MINOR" "map-repo@10.24.1.107:centreon-studio-repository/$MAJOR/$MINOR"

# Generate online documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no ubuntu@10.24.1.54"
$SSH_DOC "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-map-4 -V latest -p'"
