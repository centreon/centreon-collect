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
  put_testing_source "standard" "web" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz" centreon-api-v21.10.html
  copy_internal_rpms_to_testing "standard" "21.10" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "21.10" "el8" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  TARGETVERSION="$VERSION"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "standard" "21.10" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  promote_canary_rpms_to_unstable "standard" "21.10" "el8" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  SSH_REPO='ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com'
  put_internal_source "web" "$PROJECT-$VERSION-$RELEASE" centreon-api-v21.10.html
  $SSH_REPO aws s3 cp --acl public-read "/srv/sources/internal/web/$PROJECT-$VERSION-$RELEASE/centreon-api-v21.10.html" s3://centreon-documentation-prod/api/centreon-web/index.html
  $SSH_REPO aws cloudfront create-invalidation --distribution-id E3KVGH6VYVX7DP --paths /api/centreon-web/index.html
  TARGETVERSION='21.10'
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for image in mon-web-fresh mon-web mon-web-widgets ; do
  for distrib in centos7 centos8 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-$TARGETVERSION:$distrib"
    docker push "$REGISTRY/$image-$TARGETVERSION:$distrib"
  done
done
