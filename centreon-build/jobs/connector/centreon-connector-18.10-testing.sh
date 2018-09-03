#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-connector

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
BUILD_CENTOS7=ci.int.centreon.com:5000/mon-build-dependencies-18.10:centos7
docker pull "$BUILD_CENTOS7"

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Get version.
cd centreon-connector
git checkout --detach "$COMMIT"
cmakelists=perl/build/CMakeLists.txt
major=`grep 'set(CONNECTOR_PERL_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CONNECTOR_PERL_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CONNECTOR_PERL_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Create source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../input/$PROJECT-$VERSION.tar.gz"
cd ..

# Retrieve spec file.
cp `dirname $0`/../../packaging/connector/centreon-connector-18.10.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input output-centos7

# Copy sources to server.
SSH_REPO="ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com"
DESTDIR="/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE"
$SSH_REPO mkdir "$DESTDIR"
scp -o StrictHostKeyChecking=no "input/$PROJECT-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:$DESTDIR/"

# Copy files to server.
FILES_CENTOS7='output-centos7/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/18.10/el7/testing/x86_64/RPMS"
$SSH_REPO createrepo /srv/yum/standard/18.10/el7/testing/x86_64

# Generate doc.
# SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
# $SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-connector -V latest -p'"
