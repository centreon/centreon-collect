#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-web

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

#
# Release delivery.
#
if [ "$BUILD" '=' 'RELEASE' ] ; then
  copy_internal_source_to_testing "standard" "web" "$PROJECT-$VERSION-$RELEASE"
  copy_internal_rpms_to_testing "standard" "19.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon -V latest -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon -V latest -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon -V 19.04 -p'"
  $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon -V 19.04 -p'"
  TARGETVERSION="$VERSION"

#
# CI delivery.
#
else
  promote_canary_rpms_to_unstable "standard" "19.04" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE"
  TARGETVERSION='19.04'
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for image in mon-web-fresh mon-web mon-web-widgets ; do
  for distrib in centos7 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-$TARGETVERSION:$distrib"
    docker push "$REGISTRY/$image-19.04:$distrib"
  done
done
