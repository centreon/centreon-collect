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

# Tag and push images.
REGISTRY='registry.centreon.com'
for image in des-map-server des-map-web ; do
  for distrib in centos7 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-19.4:$distrib"
    docker push "$REGISTRY/$image-19.4:$distrib"
  done
done

# Move RPMs to unstable.
promote_canary_rpms_to_unstable "map" "19.4" "el7" "noarch" "map-web" "$PROJECT-$VERSION-$RELEASE"
promote_canary_rpms_to_unstable "map" "19.4" "el7" "noarch" "map-server" "$PROJECT-$VERSION-$RELEASE"
