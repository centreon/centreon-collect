#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <6|7>"
  exit 1
fi
CENTOS_VERSION="$1"

# Pull mon-build-dependencies container.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos$CENTOS_VERSION

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get Centreon Engine sources.
if [ \! -d centreon-engine ] ; then
  git clone https://github.com/centreon/centreon-engine
fi

# Get version.
cmakelists=centreon-engine/build/CMakeLists.txt
major=`grep 'set(CENTREON_ENGINE_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_ENGINE_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_ENGINE_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Get release.
cd centreon-engine
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

# Create source tarball.
git archive --prefix="centreon-engine-$VERSION/" "$GIT_BRANCH" | gzip > "../input/centreon-engine-$VERSION.tar.gz"
cd ..

# Retrieve spec file.
if [ \! -d packaging-centreon-engine ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon-engine.git
else
  cd packaging-centreon-engine
  git pull
  cd ..
fi
cp packaging-centreon-engine/rpm/centreon-engine.spectemplate input/

# Retrieve additional sources.
cp packaging-centreon-engine/src/centreonengine_integrate_centreon_engine2centreon.sh input/

# Build RPMs.
docker-rpm-builder dir ci.int.centreon.com:5000/mon-build-dependencies:centos$CENTOS_VERSION input output

# Copy files to server.
if [ "$CENTOS_VERSION" = 6 ] ; then
  CES_VERSION='3'
else
  CES_VERSION='4'
fi
FILES='output/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.int.centreon.com:/srv/repos/standard/$CES_VERSION/unstable/x86_64/RPMS"
DESTFILE=`ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" mktemp`
scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "root@srvi-ces-repository.int.centreon.com:$DESTFILE"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" sh $DESTFILE $CES_VERSION x86_64
