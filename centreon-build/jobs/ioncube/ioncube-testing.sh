#!/bin/sh

set -e
set -x

# Project.
PROJECT=ioncube

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
BUILD_CENTOS7=ci.int.centreon.com:5000/mon-build-dependencies-18.9:centos7
docker pull "$BUILD_CENTOS7"

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Get object file.
rm -rf "$PROJECT.tar.gz" ioncube
curl -o "$PROJECT.tar.gz" 'https://downloads.ioncube.com/loader_downloads/ioncube_loaders_lin_x86-64.tar.gz'
tar xzf "$PROJECT.tar.gz"
cp ioncube/ioncube_loader_lin_7.1.so input/

# Retrieve spec file and additional sources.
cp `dirname $0`/../../packaging/ioncube/* input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input output-centos7

# Copy files to server.
SSH_REPO="ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com"
FILES_CENTOS7='output-centos7/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/18.9/el7/testing/x86_64/RPMS"
$SSH_REPO createrepo /srv/yum/standard/18.9/el7/testing/x86_64
