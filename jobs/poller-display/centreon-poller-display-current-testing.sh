#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-poller-display

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
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
cd "$PROJECT"
VERSION=`grep mod_release www/modules/centreon-poller-display/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"

# Create source tarball.
git checkout --detach "$COMMIT"
git archive "--prefix=$PROJECT-$VERSION/" HEAD | gzip > "../input/$PROJECT-$VERSION.tar.gz"
cd ..

# Build RPMs.
cp `dirname $0`/../../packaging/poller-display/centreon-poller-display.spectemplate input/
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
REPO="ubuntu@srvi-repo.int.centreon.com"
ssh "$REPO" mkdir -p "/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE"
scp "input/$PROJECT-$VERSION.tar.gz" "$REPO:/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE/"
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp $FILES_CENTOS6 "$REPO:/srv/yum/standard/3.4/el6/testing/noarch/RPMS"
scp $FILES_CENTOS7 "$REPO:/srv/yum/standard/3.4/el7/testing/noarch/RPMS"
ssh "$REPO" createrepo /srv/yum/standard/3.4/el6/testing/noarch
ssh "$REPO" createrepo /srv/yum/standard/3.4/el7/testing/noarch

# Generate testing documentation.
DOC="root@doc-dev.int.centreon.com"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-poller-display -V latest -p'"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-poller-display -V latest -p'"
