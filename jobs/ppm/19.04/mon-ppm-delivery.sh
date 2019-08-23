#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-pp-manager

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "standard" "ppm" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "19.04" "el7" "noarch" "ppm" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  # Set Docker images as latest.
  REGISTRY='registry.centreon.com'
  for image in mon-ppm mon-ppm-autodisco ; do
    for distrib in centos7 ; do
      docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
      docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-19.04:$distrib"
      docker push "$REGISTRY/$image-19.04:$distrib"
    done
  done

  # Move RPMs to unstable.
  promote_canary_rpms_to_unstable "standard" "19.04" "el7" "noarch" "ppm" "$PROJECT-$VERSION-$RELEASE"
fi
