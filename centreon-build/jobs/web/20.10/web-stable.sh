#!/bin/sh

set -e
set -x

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
promote_rpms_from_testing_to_stable "standard" "20.10" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
promote_rpms_from_testing_to_stable "standard" "20.10" "el8" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
