#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-bi-server

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Set Docker images as latest.
REGISTRY='ci.int.centreon.com:5000'
for distrib in centos7 ; do
  # -server- image.
  docker pull "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-server-19.4:$distrib"
  docker push "$REGISTRY/des-mbi-server-19.4:$distrib"

  # -web- image.
  docker pull "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-web-19.4:$distrib"
  docker push "$REGISTRY/des-mbi-web-19.4:$distrib"
done

# Move RPMs to unstable.
promote_canary_rpms_to_unstable "mbi" "19.4" "el7" "noarch" "mbi-web" "$PROJECT-$VERSION-$RELEASE"
