#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-web

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "standard" "web" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "20.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  TARGETVERSION="$VERSION"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "standard" "20.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  TARGETVERSION='20.04'
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for image in mon-web-fresh mon-web mon-web-widgets ; do
  for distrib in centos7 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-$TARGETVERSION:$distrib"
    docker push "$REGISTRY/$image-$TARGETVERSION:$distrib"
  done
done
