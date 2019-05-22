#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-autodiscovery

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  # Copy artifacts.
  copy_internal_source_to_testing "standard" "autodisco" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "19.10" "el7" "x86_64" "autodisco" "$PROJECT-$VERSION-$RELEASE"

  # Build documentation.
  SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-auto-discovery -V latest -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-auto-discovery -V latest -p'"

  # Create entry in download-dev.
  SRCHASH=00112233445566778899aabbccddeeff
  curl "$DLDEV_URL/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION-php72&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=0"

#
# CI delivery.
#
else
  # Set Docker images as latest.
  REGISTRY='registry.centreon.com'
  for distrib in centos7 ; do
    docker pull "$REGISTRY/mon-autodisco-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/mon-autodisco-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-autodisco-19.10:$distrib"
    docker push "$REGISTRY/mon-autodisco-19.10:$distrib"
  done

  # Move RPMs to unstable.
  promote_canary_rpms_to_unstable "standard" "19.10" "el7" "x86_64" "autodisco" "$PROJECT-$VERSION-$RELEASE"
fi
