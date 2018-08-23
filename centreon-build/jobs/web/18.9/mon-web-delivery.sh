#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='ci.int.centreon.com:5000'
for image in mon-web-fresh mon-web mon-web-widgets ; do
  for distrib in centos7 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-18.9:$distrib"
    docker push "$REGISTRY/$image-18.9:$distrib"
  done
done

# Move RPMs to unstable.
move_internal_rpms_to_unstable "standard" "18.9" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
