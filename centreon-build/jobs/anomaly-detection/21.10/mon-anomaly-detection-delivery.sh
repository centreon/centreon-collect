#!/bin/sh

set -e

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-anomaly-detection

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

MAJOR=`echo $VERSION | cut -d . -f 1,2`
EL7RPMS=`echo output/noarch/*.el7.*.rpm`
EL8RPMS=`echo output/noarch/*.el8.*.rpm`

# Publish RPMs.
if [ "$BUILD" '=' 'QA' ]
then
  put_rpms "standard" "$MAJOR" "el7" "unstable" "noarch" "anomaly-detection" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "unstable" "noarch" "anomaly-detection" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
elif [ "$BUILD" '=' 'RELEASE' ]
then
  # Create entry in download-dev.
  SRCHASH=00112233445566778899aabbccddeeff
  curl "$DLDEV_URL/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=0"
  copy_internal_source_to_testing "standard" "anomaly-detection" "$PROJECT-$VERSION-$RELEASE"
  put_rpms "standard" "$MAJOR" "el7" "testing" "noarch" "anomaly-detection" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "testing" "noarch" "anomaly-detection" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
elif [ "$BUILD" '=' 'CI' ]
then
  put_rpms "standard" "$MAJOR" "el7" "canary" "noarch" "anomaly-detection" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "canary" "noarch" "anomaly-detection" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
fi
