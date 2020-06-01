#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=ioncube

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
BUILD_CENTOS7=registry.centreon.com/mon-build-dependencies-19.10:centos7
docker pull "$BUILD_CENTOS7"
BUILD_CENTOS8=registry.centreon.com/mon-build-dependencies-20.04:centos8
docker pull "$BUILD_CENTOS8"

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7
rm -rf output-centos8
mkdir output-centos8

# Get object file.
rm -rf "$PROJECT.tar.gz" ioncube
curl -o "$PROJECT.tar.gz" 'https://downloads.ioncube.com/loader_downloads/ioncube_loaders_lin_x86-64.tar.gz'
tar xzf "$PROJECT.tar.gz"
cp ioncube/ioncube_loader_lin_7.2.so input/

# Retrieve spec file and additional sources.
cp `dirname $0`/../../../packaging/ioncube/php72/* input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_CENTOS7" input output-centos7
docker-rpm-builder dir --sign-with `dirname $0`/../../ces.key "$BUILD_CENTOS8" input output-centos8

# Copy files to server.
put_testing_rpms "standard" "19.10" "el7" "x86_64" "ioncube" "ioncube" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "20.04" "el7" "x86_64" "ioncube" "ioncube" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "20.04" "el8" "x86_64" "ioncube" "ioncube" output-centos8/x86_64/*.rpm
put_testing_rpms "standard" "20.10" "el7" "x86_64" "ioncube" "ioncube" output-centos7/x86_64/*.rpm
put_testing_rpms "standard" "20.10" "el8" "x86_64" "ioncube" "ioncube" output-centos8/x86_64/*.rpm
