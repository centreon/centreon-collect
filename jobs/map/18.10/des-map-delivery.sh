#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-map4-web-client

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Tag and push images.
REGISTRY='ci.int.centreon.com:5000'
for image in des-map-server des-map-web ; do
  for distrib in centos7 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-3.4:$distrib"
    docker push "$REGISTRY/$image-3.4:$distrib"
  done
done

# Move RPMs to unstable.
move_internal_rpms_to_unstable "map" "18.10" "el7" "noarch" "map" "$PROJECT-$VERSION-$RELEASE"
