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

MAJOR=`echo $VERSION | cut -d . -f 1,2`

SERVEREL7RPMS=`echo output/noarch/centreon-map-server-$MAJOR*.el7.*.rpm`
SERVEREL8RPMS=`echo output/noarch/centreon-map-server-$MAJOR*.el8.*.rpm`
NGEL7RPMS=`echo output/noarch/centreon-map-server-ng*.el7.*.rpm`
NGEL8RPMS=`echo output/noarch/centreon-map-server-ng*.el8.*.rpm`
WEBEL7RPMS=`echo output/noarch/centreon-map-web*.el7.*.rpm`
WEBEL8RPMS=`echo output/noarch/centreon-map-web*.el8.*.rpm`

# Publish RPMs.
if [ "$BUILD" '=' 'QA' ]
then
  put_rpms "map" "$MAJOR" "el7" "unstable" "noarch" "map-server" "centreon-map-server-$VERSIONSERVER-$RELEASE" $SERVEREL7RPMS
  put_rpms "map" "$MAJOR" "el8" "unstable" "noarch" "map-server" "centreon-map-server-$VERSIONSERVER-$RELEASE" $SERVEREL8RPMS
  put_rpms "map" "$MAJOR" "el7" "unstable" "noarch" "map-server-ng" "centreon-map-server-$VERSIONSERVER-$RELEASE" $NGEL7RPMS
  put_rpms "map" "$MAJOR" "el8" "unstable" "noarch" "map-server-ng" "centreon-map-server-$VERSIONSERVER-$RELEASE" $NGEL8RPMS
  put_rpms "map" "$MAJOR" "el7" "unstable" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $WEBEL7RPMS
  put_rpms "map" "$MAJOR" "el8" "unstable" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $WEBEL8RPMS
elif [ "$BUILD" '=' 'RELEASE' ]
then
  put_rpms "map" "$MAJOR" "el7" "testing" "noarch" "map-server" "centreon-map-server-$VERSIONSERVER-$RELEASE" $SERVEREL7RPMS
  put_rpms "map" "$MAJOR" "el8" "testing" "noarch" "map-server" "centreon-map-server-$VERSIONSERVER-$RELEASE" $SERVEREL8RPMS
  put_rpms "map" "$MAJOR" "el7" "testing" "noarch" "map-server-ng" "centreon-map-server-$VERSIONSERVER-$RELEASE" $NGEL7RPMS
  put_rpms "map" "$MAJOR" "el8" "testing" "noarch" "map-server-ng" "centreon-map-server-$VERSIONSERVER-$RELEASE" $NGEL8RPMS
  put_rpms "map" "$MAJOR" "el7" "testing" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $WEBEL7RPMS
  put_rpms "map" "$MAJOR" "el8" "testing" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $WEBEL8RPMS
elif [ "$BUILD" '=' 'CI' ]
then
  put_rpms "map" "$MAJOR" "el7" "canary" "noarch" "map-server" "centreon-map-server-$VERSIONSERVER-$RELEASE" $SERVEREL7RPMS
  put_rpms "map" "$MAJOR" "el8" "canary" "noarch" "map-server" "centreon-map-server-$VERSIONSERVER-$RELEASE" $SERVEREL8RPMS
  put_rpms "map" "$MAJOR" "el7" "canary" "noarch" "map-server-ng" "centreon-map-server-$VERSIONSERVER-$RELEASE" $NGEL7RPMS
  put_rpms "map" "$MAJOR" "el8" "canary" "noarch" "map-server-ng" "centreon-map-server-$VERSIONSERVER-$RELEASE" $NGEL8RPMS
  put_rpms "map" "$MAJOR" "el7" "canary" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $WEBEL7RPMS
  put_rpms "map" "$MAJOR" "el8" "canary" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $WEBEL8RPMS
fi

