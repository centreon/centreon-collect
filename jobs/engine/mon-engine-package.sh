#!/bin/sh

# Pull mon-build-dependencies container.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos6

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
  git clone http://gitbot:gitbot@git.merethis.net/packaging-centreon-engine.git
else
  cd packaging-centreon-engine
  git pull
  cd ..
fi
cp packaging-centreon-engine/rpm/centreon-engine.spectemplate input/

# Retrieve additional sources.
cp packaging-centreon-engine/src/centreonengine_integrate_centreon_engine2centreon.sh input/

# Build RPMs.
docker-rpm-builder dir ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output

# Copy files to server.
CES_VERSION='3.0'
FILES='output/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.merethis.net:/srv/repos/standard/$CES_VERSION/testing/x86_64/RPMS"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.merethis.net" createrepo "root@srvi-ces-repository.merethis.net:/srv/repos/standard/$CES_VERSION/testing/x86_64/"
