#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-engine

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
cd $PROJECT
git checkout --detach "$COMMIT"
cmakelists=build/CMakeLists.txt
major=`grep 'set(CENTREON_ENGINE_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_ENGINE_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_ENGINE_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Create source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../input/$PROJECT-$VERSION.tar.gz"
cd ..

# Retrieve spectemplate and additional source files.
cp `dirname $0`/../../packaging/engine/centreon-engine-18.10.spectemplate input/
cp `dirname $0`/../../packaging/engine/centreonengine_integrate_centreon_engine2centreon.sh input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input output-centos7

# Copy files to server.
put_testing_source "standard" "engine" "$PROJECT-$VERSION-$RELEASE" "input/$PROJECT-$VERSION.tar.gz"
put_testing_rpms "standard" "18.10" "el7" "x86_64" "engine" "$PROJECT-$VERSION-$RELEASE" output-centos7/x86_64/*.rpm

# Generate doc.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos $PROJECT -V latest -p'"
