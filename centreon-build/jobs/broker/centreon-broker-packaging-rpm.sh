#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$COMMIT" -o -z "$REPO" ] ; then
  echo "You need to specify COMMIT and REPO environment variables."
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
cd centreon-broker
git checkout --detach "$COMMIT"
cmakelists=centreon-broker/build/CMakeLists.txt
major=`grep 'set(CENTREON_BROKER_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_BROKER_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_BROKER_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Create source tarball.
git archive --prefix="centreon-broker-$VERSION/" HEAD | gzip > "../input/centreon-broker-$VERSION.tar.gz"
cd ..

# Retrieve spec file.
if [ \! -d packaging-centreon-broker ] ; then
  git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon-broker.git
else
  cd packaging-centreon-broker
  git checkout master
  git pull
  cd ..
fi
cd packaging-centreon-broker
git checkout --detach "origin/$VERSION"
cd ..
cp packaging-centreon-broker/rpm/centreon-broker.spec input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
FILES_CENTOS6='output-centos6/x86_64/*.rpm'
FILES_CENTOS7='output-centos7/x86_64/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el6/$REPO/x86_64/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el7/$REPO/x86_64/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el6/$REPO/x86_64
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el7/$REPO/x86_64
