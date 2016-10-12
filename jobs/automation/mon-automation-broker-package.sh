#!/bin/sh

set -e
set -x

# Check arguments.
if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <centos6|centos7>"
  exit 1
fi
DISTRIB="$1"

# Pull mon-build-dependencies container.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get version.
cmakelists=centreon-discovery-engine/build/CMakeLists.txt
major=`grep 'set(CENTREON_DISCOVERY_ENGINE_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_DISCOVERY_ENGINE_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_DISCOVERY_ENGINE_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Get release.
cd centreon-discovery-engine
commit=`git log -1 "$GIT_COMMIT" --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$commit"

# Create source tarball.
git archive --prefix="centreon-discovery-engine-$VERSION/" "$GIT_BRANCH" | gzip > "../input/centreon-discovery-engine-$VERSION.tar.gz"

# Retrieve spec file.
cp packaging/centreon-discovery-engine.spectemplate ../input/
cd ..

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:$DISTRIB input output

# Copy files to server.
if [ "$DISTRIB" = centos6 ] ; then
  REPO='standard/dev/el6/unstable/x86_64'
else
  REPO='standard/dev/el7/unstable/x86_64'
fi
FILES='output/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "root@srvi-ces-repository.int.centreon.com:/srv/repos/$REPO/RPMS"
DESTFILE=`ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" mktemp`
scp -o StrictHostKeyChecking=no `dirname $0`/../updaterepo.sh "root@srvi-ces-repository.int.centreon.com:$DESTFILE"
ssh -o StrictHostKeyChecking=no "root@srvi-ces-repository.int.centreon.com" sh $DESTFILE $REPO
