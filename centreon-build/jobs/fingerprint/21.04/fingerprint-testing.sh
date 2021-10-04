#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-fingerprint

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
BUILD_CENTOS7="registry.centreon.com/mon-build-dependencies-21.04:centos7"
docker pull "$BUILD_CENTOS7"
BUILD_CENTOS8="registry.centreon.com/mon-build-dependencies-21.04:centos8"
docker pull "$BUILD_CENTOS8"

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
cp -r `dirname $0`/../../../packaging/fingerprint/* input
rm -rf output-centos7
mkdir output-centos7
rm -rf output-centos8
mkdir output-centos8

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_CENTOS7" input output-centos7
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_CENTOS8" input output-centos8

# Copy files to server.
MAJOR=`echo $VERSION | cut -d . -f 1,2`
EL7RPMS=`echo output-centos7/noarch/*.el7.*.rpm`
EL8RPMS=`echo output-centos8/noarch/*.el8.*.rpm`

put_rpms "standard" "$MAJOR" "el7" "testing" "x86_64" "fingerprint" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
put_rpms "standard" "$MAJOR" "el8" "testing" "x86_64" "fingerprint" "$PROJECT-$VERSION-$RELEASE" $EL7RPMS
