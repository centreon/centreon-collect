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
  copy_internal_source_to_testing "standard" "awie" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "19.04" "el7" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE"
  SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-awie -V latest -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-awie -V latest -p'"

#
# CI delivery.
#
else
  # Set Docker images as latest.
  REGISTRY='registry.centreon.com'
  for distrib in centos7 ; do
    docker pull "$REGISTRY/mon-awie-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/mon-awie-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-awie-19.04:$distrib"
    docker push "$REGISTRY/mon-awie-19.04:$distrib"
  done

  # Move RPMs to unstable.
  promote_canary_rpms_to_unstable "standard" "19.04" "el7" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE"
fi
