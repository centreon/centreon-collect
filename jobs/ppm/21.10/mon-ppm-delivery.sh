#!/bin/sh

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-pp-manager

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
  put_rpms "standard" "$MAJOR" "el7" "unstable" "noarch" "ppm" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "unstable" "noarch" "ppm" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
  TARGETVERSION='21.10'
elif [ "$BUILD" '=' 'RELEASE' ]
then
  copy_internal_source_to_testing "standard" "ppm" "$PROJECT-$VERSION-$RELEASE"
  put_rpms "standard" "$MAJOR" "el7" "testing" "noarch" "ppm" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "standard" "$MAJOR" "el8" "testing" "noarch" "ppm" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
  TARGETVERSION="$VERSION"
fi

# Set Docker images as latest.
REGISTRY='registry.centreon.com'
for image in mon-ppm mon-ppm-autodisco ; do
  for distrib in centos7 centos8 ; do
    docker pull "$REGISTRY/$image-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/$image-$VERSION-$RELEASE:$distrib" "$REGISTRY/$image-$TARGETVERSION:$distrib"
    docker push "$REGISTRY/$image-$TARGETVERSION:$distrib"
  done
done
