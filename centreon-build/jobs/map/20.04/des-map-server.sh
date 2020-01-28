#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map

# Check arguments.
if [ -z "$VERSION" -o -z "$VERSIONSERVER" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION, VERSIONSERVER and RELEASE environment variables."
  exit 1
fi

# Fetch sources.
rm -rf "$PROJECT-server-$VERSION.tar.gz" "$PROJECT-server-$VERSION"
get_internal_source "map/$PROJECT-server-$VERSIONSERVER-$RELEASE/$PROJECT-server-$VERSIONSERVER.tar.gz"

# Create and populate container.
BUILD_IMAGE="registry.centreon.com/map-build-dependencies-20.04:centos7"
docker pull "$BUILD_IMAGE"
containerid=`docker create -e "VERSION=$VERSIONSERVER" $BUILD_IMAGE /usr/local/bin/server.sh`
docker cp `dirname $0`/des-map-server.container.sh "$containerid:/usr/local/bin/server.sh"
docker cp "$PROJECT-server-$VERSIONSERVER.tar.gz" "$containerid:/usr/local/src/"

# Run container.
docker start -a "$containerid"

# Copy artifacts.
rm -rf "$PROJECT-server-$VERSIONSERVER"
docker cp "$containerid:/usr/local/src/$PROJECT-server-$VERSIONSERVER" "$PROJECT-server-$VERSIONSERVER"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Upload artifacts.
FILES_MAP_SERVER="$PROJECT-server-$VERSIONSERVER/map-server-parent/map-server-packaging/target/rpm/centreon-map-server/RPMS/noarch/"'*.rpm'
put_internal_rpms "20.04" "el7" "noarch" "map-server" "$PROJECT-server-$VERSIONSERVER-$RELEASE" $FILES_MAP_SERVER
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO rpm --resign "/srv/yum/internal/20.04/el7/noarch/map-server/$PROJECT-server-$VERSIONSERVER-$RELEASE/*.rpm"
$SSH_REPO createrepo "/srv/yum/internal/20.04/el7/noarch/map-server/$PROJECT-server-$VERSIONSERVER-$RELEASE"
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "map" "20.04" "el7" "noarch" "map-server" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
fi
