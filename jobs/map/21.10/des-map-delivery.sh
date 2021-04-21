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


#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  if [ "$PRODUCT" '=' 'desktop' -o "$PRODUCT" '=' 'all' ] ; then
    copy_internal_source_to_testing "map" "map-desktop" "$PROJECT-desktop-$VERSION-$RELEASE"
  fi
  if [ "$PRODUCT" '=' 'server' -o "$PRODUCT" '=' 'all' ] ; then
    copy_internal_rpms_to_testing "map" "21.10" "el7" "noarch" "map-server" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
    copy_internal_rpms_to_testing "map" "21.10" "el7" "noarch" "map-server-ng" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
    copy_internal_rpms_to_testing "map" "21.10" "el8" "noarch" "map-server" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
    copy_internal_rpms_to_testing "map" "21.10" "el8" "noarch" "map-server-ng" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
  fi
  if [ "$PRODUCT" '=' 'web' -o "$PRODUCT" '=' 'all' ] ; then
    copy_internal_rpms_to_testing "map" "21.10" "el7" "noarch" "map-web" "$PROJECT-web-$VERSIONWEB-$RELEASE"
    copy_internal_rpms_to_testing "map" "21.10" "el8" "noarch" "map-web" "$PROJECT-web-$VERSIONWEB-$RELEASE"
  fi
  TARGETVERSION="$VERSION"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "map" "21.10" "el7" "noarch" "map-web" "$PROJECT-web-$VERSIONWEB-$RELEASE"
  promote_canary_rpms_to_unstable "map" "21.10" "el7" "noarch" "map-server" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
  promote_canary_rpms_to_unstable "map" "21.10" "el7" "noarch" "map-server-ng" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
  promote_canary_rpms_to_unstable "map" "21.10" "el8" "noarch" "map-web" "$PROJECT-web-$VERSIONWEB-$RELEASE"
  promote_canary_rpms_to_unstable "map" "21.10" "el8" "noarch" "map-server" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
  promote_canary_rpms_to_unstable "map" "21.10" "el8" "noarch" "map-server-ng" "$PROJECT-server-$VERSIONSERVER-$RELEASE"
  TARGETVERSION='21.10'
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for image in des-map-server des-map-web ; do
  for distrib in centos7 centos8 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-$TARGETVERSION:$distrib"
    docker push "$REGISTRY/$image-$TARGETVERSION:$distrib"
  done
done
