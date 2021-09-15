#!/bin/sh

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-bam-server

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
  copy_internal_source_to_testing "standard" "bam" "$PROJECT-$VERSION-$RELEASE"
  put_rpms "business" "$MAJOR" "el7" "unstable" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "business" "$MAJOR" "el8" "unstable" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
  TARGETVERSION='21.10'
elif [ "$BUILD" '=' 'RELEASE' ]
then
  put_rpms "business" "$MAJOR" "el7" "testing" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
  put_rpms "business" "$MAJOR" "el8" "testing" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE" $EL8RPMS
  TARGETVERSION="$VERSION"
  # Set Docker images as latest.
  REGISTRY='registry.centreon.com'
  for distrib in centos7 centos8 ; do
    docker pull "$REGISTRY/des-bam-$VERSION-$RELEASE:$distrib"
    docker tag "$REGISTRY/des-bam-$VERSION-$RELEASE:$distrib" "$REGISTRY/des-bam-$TARGETVERSION:$distrib"
    docker push "$REGISTRY/des-bam-$TARGETVERSION:$distrib"
done
fi


