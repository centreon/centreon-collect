#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  put_testing_source "standard" "web" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz" centreon-api-v2.html
  copy_internal_rpms_to_testing "standard" "20.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  TARGETVERSION="$VERSION"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "standard" "20.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
  put_internal_source "web" "$PROJECT-$VERSION-$RELEASE" centreon-api-v2.html
  $SSH_REPO aws s3 cp --acl public-read "/srv/sources/internal/web/$PROJECT-$VERSION-$RELEASE/centreon-api-v2.html" s3://centreon-documentation/centreon-web/centreon-api-v2.html
  $SSH_REPO aws cloudfront create-invalidation --distribution-id E1W5L9V83QVVLX --paths /centreon-web/centreon-api-v2.html
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
