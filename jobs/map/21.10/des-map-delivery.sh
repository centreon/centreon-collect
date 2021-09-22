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

ls -lart

MAJOR=`echo $VERSION | cut -d . -f 1,2`
ls -lart output/noarch/
SERVEREL7RPMS=`echo output/noarch/*.el7.*.rpm`
SERVEREL8RPMS=`echo output/noarch/*.el8.*.rpm`

# # Publish RPMs.
# if [ "$BUILD" '=' 'QA' -o "$BUILD" '=' 'CI' ] ; then
#     put_rpms "business" "$MAJOR" "el7" "unstable" "noarch" "map-server" "centreon-map-$VERSIONSERVER-$RELEASE" $EL7RPMS
#     put_rpms "business" "$MAJOR" "el8" "unstable" "noarch" "map-server" "centreon-map-$VERSIONSERVER-$RELEASE" $EL8RPMS
#     put_rpms "business" "$MAJOR" "el7" "unstable" "noarch" "map-server-ng" "centreon-map-$VERSIONSERVER-$RELEASE" $EL7RPMS
#     put_rpms "business" "$MAJOR" "el8" "unstable" "noarch" "map-server-ng" "centreon-map-$VERSIONSERVER-$RELEASE" $EL8RPMS
#     put_rpms "business" "$MAJOR" "el7" "unstable" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $EL7RPMS
#     put_rpms "business" "$MAJOR" "el8" "unstable" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $EL8RPMS
# fi
# elif [ "$BUILD" '=' 'RELEASE' ]
# then
#   put_rpms "business" "$MAJOR" "el7" "testing" "noarch" "map-server" "centreon-map-$VERSIONSERVER-$RELEASE" $EL7RPMS
#   put_rpms "business" "$MAJOR" "el8" "testing" "noarch" "map-server" "centreon-map-$VERSIONSERVER-$RELEASE" $EL8RPMS
#   put_rpms "business" "$MAJOR" "el7" "testing" "noarch" "map-server-ng" "centreon-map-$VERSIONSERVER-$RELEASE" $EL7RPMS
#   put_rpms "business" "$MAJOR" "el8" "testing" "noarch" "map-server-ng" "centreon-map-$VERSIONSERVER-$RELEASE" $EL8RPMS
#   put_rpms "business" "$MAJOR" "el7" "testing" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $EL7RPMS
#   put_rpms "business" "$MAJOR" "el8" "testing" "noarch" "map-web" "centreon-map-web-$VERSIONWEB-$RELEASE" $EL8RPMS
# fi