#!/bin/sh


. `dirname $0`/../../common.sh

# Project.
export PROJECT=centreon-web

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Put sources online.
upload_tarball_for_download "$PROJECT" "$VERSION" "/srv/sources/standard/stable/$PROJECT-$VERSION-$RELEASE/$PROJECT-$VERSION.tar.gz" "s3://centreon-download/public/centreon/$PROJECT-$VERSION.tar.gz"

# Move RPMs to the stable repository.
MAJOR=`echo $VERSION | cut -d . -f 1,2`

promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el7" "noarch" "web" "centreon-web-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "$MAJOR" "el8" "noarch" "web" "centreon-web-$VERSION-$RELEASE"