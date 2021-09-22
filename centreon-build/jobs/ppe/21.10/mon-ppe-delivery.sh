#!/bin/sh

set -e

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

MAJOR=`echo $VERSION | cut -d . -f 1,2`
EL7RPMS=`echo output/noarch/*.el7.*.rpm`
EL8RPMS=`echo output/noarch/*.el8.*.rpm`

# Publish RPMs.
if [ "$BUILD" '=' 'QA' -o "$BUILD" '=' 'CI' ]
then
  put_rpms "standard" "$MAJOR" "el7" "unstable" "noarch" "export" "centreon-export-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "unstable" "noarch" "export" "centreon-export-$VERSION-$RELEASE" $EL8RPMS
elif [ "$BUILD" '=' 'RELEASE' ]
then
  copy_internal_source_to_testing "mbi" "mbi" "$PROJECT-$VERSION-$RELEASE"
  put_rpms "standard" "$MAJOR" "el7" "testing" "noarch" "export" "centreon-export-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "testing" "noarch" "export" "centreon-export-$VERSION-$RELEASE" $EL8RPMS
fi

# # Set Docker images as latest.
# REGISTRY='registry.centreon.com'
# for distrib in centos7 ; do
#   docker pull "$REGISTRY/mon-ppe-$VERSION-$RELEASE:$distrib"
#   docker tag "$REGISTRY/mon-ppe-$VERSION-$RELEASE:$distrib" "$REGISTRY/mon-ppe-21.10:$distrib"
#   docker push "$REGISTRY/mon-ppe-21.10:$distrib"
# done
