#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-engine

# Check arguments.
if [ -z "$COMMIT" ] ; then
  echo "You need to specify COMMIT environment variable."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos6
docker pull ci.int.centreon.com:5000/mon-build-dependencies:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos6
mkdir output-centos6
rm -rf output-centos7
mkdir output-centos7

# Get version.
cd centreon-engine
git checkout --detach "$COMMIT"
cmakelists=build/CMakeLists.txt
major=`grep 'set(CENTREON_ENGINE_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_ENGINE_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_ENGINE_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Create source tarball.
git archive --prefix="centreon-engine-$VERSION/" HEAD | gzip > "../input/centreon-engine-$VERSION.tar.gz"
cd ..

# Retrieve spec file.
if [ \! -d packaging-centreon-engine ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon-engine.git
else
  cd packaging-centreon-engine
  git checkout master
  git pull
  cd ..
fi
cd packaging-centreon-engine
git checkout --detach "origin/$VERSION"
cd ..
cp packaging-centreon-engine/rpm/centreon-engine.spec input/
RELEASE=`cat packaging-centreon-engine/rpm/centreon-engine.spec | grep Release | cut -d ' ' -f 9 | cut -d '%' -f 1`

# Retrieve additional sources.
cp packaging-centreon-engine/src/centreonengine_integrate_centreon_engine2centreon.sh input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy sources to server.
SSH_REPO="ssh -o StrictHostKeyChecking=no ubuntu@srvi-repo.int.centreon.com"
DESTDIR="/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE"
$SSH_REPO mkdir "$DESTDIR"
scp -o StrictHostKeyChecking=no "input/$PROJECT-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:$DESTDIR/"

# Copy files to server.
FILES_CENTOS6='output-centos6/x86_64/*.rpm'
FILES_CENTOS7='output-centos7/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el6/testing/x86_64/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el7/testing/x86_64/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el6/testing/x86_64
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el7/testing/x86_64

# Generate doc.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-engine -V 1.7 -p'"
