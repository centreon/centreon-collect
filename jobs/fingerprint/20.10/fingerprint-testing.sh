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
BUILD_CENTOS7="registry.centreon.com/mon-build-dependencies-$VERSION:centos7"
docker pull "$BUILD_CENTOS7"
BUILD_CENTOS8="registry.centreon.com/mon-build-dependencies-$VERSION:centos8"
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
put_testing_rpms "standard" "20.10" "el7" "x86_64" "fingerprint" "fingerprint" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "20.10" "el8" "x86_64" "fingerprint" "fingerprint" output-centos8/x86_64/*.rpm