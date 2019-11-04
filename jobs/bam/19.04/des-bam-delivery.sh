#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-bam-server

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "bam" "bam" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "bam" "19.04" "el7" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE"
  SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-bam -V latest -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-bam -V latest -p'"
  TARGETVERSION="$VERSION"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "bam" "19.04" "el7" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE"
  TARGETVERSION='19.04'
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for distrib in centos7 ; do
  docker pull "$REGISTRY/des-bam-$VERSION-$RELEASE:$distrib"
  docker tag "$REGISTRY/des-bam-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-bam-$TARGETVERSION:$distrib"
  docker push "$REGISTRY/des-bam-$TARGETVERSION:$distrib"
done
