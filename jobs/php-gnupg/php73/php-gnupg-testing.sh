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
BUILD_CENTOS7=registry.centreon.com/mon-build-dependencies-21.04:centos7
docker pull "$BUILD_CENTOS7"
BUILD_CENTOS8=registry.centreon.com/mon-build-dependencies-21.04:centos8
docker pull "$BUILD_CENTOS8"

# Create input and output directories for docker-rpm-builder.
rm -rf input
cp -r `dirname $0`/../../../packaging/php-gnupg/php73 input
rm -rf output-centos7
mkdir output-centos7
rm -rf output-centos8
mkdir output-centos8

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_CENTOS7" input output-centos7
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_CENTOS8" input output-centos8

# Copy files to server.
put_testing_rpms "standard" "19.10" "el7" "x86_64" "php-gnupg" "php-gnupg" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "20.04" "el7" "x86_64" "php-gnupg" "php-gnupg" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "20.04" "el8" "x86_64" "php-gnupg" "php-gnupg" output-centos8/x86_64/*.rpm
put_testing_rpms "standard" "20.10" "el7" "x86_64" "php-gnupg" "php-gnupg" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "20.10" "el8" "x86_64" "php-gnupg" "php-gnupg" output-centos8/x86_64/*.rpm
put_testing_rpms "standard" "21.04" "el7" "x86_64" "php-gnupg" "php-gnupg" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "21.04" "el8" "x86_64" "php-gnupg" "php-gnupg" output-centos8/x86_64/*.rpm
