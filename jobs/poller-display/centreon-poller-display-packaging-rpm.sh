#!/bin/sh

set -e
set -x

# Check arguments.
if [ -z "$COMMIT" -o -z "$REPO" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT, REPO and RELEASE environment variables."
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
cd centreon-poller-display
VERSION=`grep mod_release www/modules/centreon-poller-display/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Create source tarball.
git checkout --detach "$COMMIT"
git archive --prefix=centreon-poller-display-$VERSION/ HEAD | gzip > ../input/centreon-poller-display-$VERSION.tar.gz
cd ..

# Build RPMs.
cp `dirname $0`/../../packaging/poller-display/centreon-poller-display.spectemplate input/
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el6/$REPO/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/3.4/el7/$REPO/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el6/$REPO/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/3.4/el7/$REPO/noarch
