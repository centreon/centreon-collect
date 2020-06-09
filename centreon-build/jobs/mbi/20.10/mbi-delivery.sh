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
  copy_internal_rpms_to_testing "mbi" "20.10" "el7" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "mbi" "20.10" "el8" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE"

#
# CI delivery.
#
else
  # Set Docker images as latest.
  REGISTRY='registry.centreon.com'
  for distrib in centos7 centos8 ; do
    # -server- image.
    docker pull "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-server-20.10:$distrib"
    docker push "$REGISTRY/des-mbi-server-20.10:$distrib"

    # -web- image.
    docker pull "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-web-20.10:$distrib"
    docker push "$REGISTRY/des-mbi-web-20.10:$distrib"
  done

  # Move RPMs to unstable.
  promote_canary_rpms_to_unstable "mbi" "20.10" "el7" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE"
  promote_canary_rpms_to_unstable "mbi" "20.10" "el8" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE"
fi
