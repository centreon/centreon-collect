#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-awie

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  # Copy build artifacts.
  copy_internal_source_to_testing "standard" "awie" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "19.10" "el7" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE"

  # Generate documentation.
  SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-awie -V latest -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-awie -V latest -p'"

  # Create entry in download-dev.
  SRCHASH=00112233445566778899aabbccddeeff
  curl "$DLDEV_URL/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=0"

  # Docker image target version.
  TARGETVERSION="$VERSION"

#
# CI delivery.
#
else
  # Move RPMs to unstable.
  promote_canary_rpms_to_unstable "standard" "19.10" "el7" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE"

  # Docker image target version.
  TARGETVERSION="19.10"
fi

# Tag Docker images.
REGISTRY='registry.centreon.com'
for distrib in centos7 ; do
  docker pull "$REGISTRY/mon-awie-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/mon-awie-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-awie-$TARGETVERSION:$distrib"
  docker push "$REGISTRY/mon-awie-$TARGETVERSION:$distrib"
done
