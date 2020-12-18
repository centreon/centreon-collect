#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-nrpe
export VERSION=3.2.1

# Check arguments.
if [ -z "$RELEASE" ] ; then
  echo "You need to specify RELEASE environment variable."
  exit 1
fi

# Pull build image.
BUILD_CENTOS7=registry.centreon.com/mon-build-dependencies-19.04:centos7
docker pull "$BUILD_CENTOS7"

# Create input and output directories.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Get source tarball.
curl -Lo input/nrpe-3.2.1.tar.gz 'https://github.com/NagiosEnterprises/nrpe/releases/download/nrpe-3.2.1/nrpe-3.2.1.tar.gz'

# Get packaging files.
cp `dirname $0`/../../packaging/nrpe/* input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input output-centos7

# Copy files to server.
put_testing_rpms "standard" "3.4" "el7" "x86_64" "nrpe" "$PROJECT-$VERSION-$RELEASE" output-centos7/x86_64/*.rpm
