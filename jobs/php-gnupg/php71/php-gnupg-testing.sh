#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=php-gnupg

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
BUILD_CENTOS7=registry.centreon.com/mon-build-dependencies-19.04:centos7
docker pull "$BUILD_CENTOS7"

# Create input and output directories for docker-rpm-builder.
rm -rf input
cp -r `dirname $0`/../../../packaging/php-gnupg/php71 input
rm -rf output-centos7
mkdir output-centos7

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_CENTOS7" input output-centos7

# Copy files to server.
put_testing_rpms "standard" "19.04" "el7" "x86_64" "php-gnupg" "php-gnupg" output-centos7/x86_64/*.rpm
