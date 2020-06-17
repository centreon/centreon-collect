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
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos7|centos8|...>"
  exit 1
fi
DISTRIB="$1"

# Fetch sources.
rm -rf "$PROJECT-server-$VERSIONSERVER.tar.gz" "$PROJECT-server-$VERSIONSERVER"
get_internal_source "map/$PROJECT-server-$VERSIONSERVER-$RELEASE/$PROJECT-server-$VERSIONSERVER.tar.gz"
tar xzf "$PROJECT-server-$VERSIONSERVER.tar.gz"
if [ "$DISTRIB" = 'centos8' ] ; then
  sed -i 's@el7</project.release>@el8</project.release>@g' "$PROJECT-server-$VERSIONSERVER/map-server-parent/map-server-packaging/pom.xml"
  rm -rf "$PROJECT-server-$VERSIONSERVER.tar.gz"
  tar czf "$PROJECT-server-$VERSIONSERVER.tar.gz" "$PROJECT-server-$VERSIONSERVER"
fi

# Create and populate container.
BUILD_IMAGE="registry.centreon.com/map-build-dependencies-20.10:$DISTRIB"
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
if [ "$DISTRIB" = 'centos7' ] ; then
  DISTRIB=el7
elif [ "$DISTRIB" = 'centos8' ] ; then
  DISTRIB=el8
else
  echo "Unsupported distribution $DISTRIB."
  exit 1
fi
FILES_MAP_SERVER="$PROJECT-server-$VERSIONSERVER/map-server-parent/map-server-packaging/target/rpm/centreon-map-server/RPMS/noarch/"'*.rpm'
put_internal_rpms "20.10" "$DISTRIB" "noarch" "map-server" "$PROJECT-server-$VERSIONSERVER-$RELEASE" $FILES_MAP_SERVER
SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
$SSH_REPO rpm --resign "/srv/yum/internal/20.10/$DISTRIB/noarch/map-server/$PROJECT-server-$VERSIONSERVER-$RELEASE/*.rpm"
$SSH_REPO createrepo "/srv/yum/internal/20.10/$DISTRIB/noarch/map-server/$PROJECT-server-$VERSIONSERVER-$RELEASE"
if [ "$BUILD" '=' 'REFERENCE' ] ; then
  copy_internal_rpms_to_canary "map" "20.10" "$DISTRIB" "noarch" "map-server" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
fi
