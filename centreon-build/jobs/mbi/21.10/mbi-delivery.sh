#!/bin/sh

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-mbi

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

MAJOR=`echo $VERSION | cut -d . -f 1,2`
EL7RPMS=`echo output/noarch/*.el7.*.rpm`
EL8RPMS=`echo output/noarch/*.el8.*.rpm`

# Publish RPMs.
if [ "$BUILD" '=' 'QA' ]
then
  # Set Docker images as latest.
  REGISTRY='registry.centreon.com'
  for distrib in centos7 centos8 ; do
    # -server- image.
    docker pull "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/des-mbi-server-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-server-21.10:$distrib"
    docker push "$REGISTRY/des-mbi-server-21.10:$distrib"

    # -web- image.
    docker pull "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/des-mbi-web-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-mbi-web-21.10:$distrib"
    docker push "$REGISTRY/des-mbi-web-21.10:$distrib"
  done
  put_rpms "business" "$MAJOR" "el7" "unstable" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "business" "$MAJOR" "el8" "unstable" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS

elif [ "$BUILD" '=' 'RELEASE' ]
then
  copy_internal_source_to_testing "mbi" "mbi" "$PROJECT-$VERSION-$RELEASE"
  put_rpms "business" "$MAJOR" "el7" "testing" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "business" "$MAJOR" "el8" "testing" "noarch" "mbi" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
fi