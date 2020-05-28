#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-mbi

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "mbi" "mbi" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "mbi" "19.04" "el7" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  # Set Docker images as latest.
  REGISTRY='registry.centreon.com'
  for distrib in centos7 ; do
    # -server- image.
    docker pull "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-server-19.04:$distrib"
    docker push "$REGISTRY/des-mbi-server-19.04:$distrib"

    # -web- image.
    docker pull "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-web-19.04:$distrib"
    docker push "$REGISTRY/des-mbi-web-19.04:$distrib"
  done

  # Move RPMs to unstable.
  promote_canary_rpms_to_unstable "mbi" "19.04" "el7" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE"
fi
