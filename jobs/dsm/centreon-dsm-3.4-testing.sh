#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-dsm

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies-3.4:centos6
docker pull registry.centreon.com/mon-build-dependencies-3.4:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos6
mkdir output-centos6
rm -rf output-centos7
mkdir output-centos7

# Get version.
cd "$PROJECT"
git checkout --detach "$COMMIT"
export VERSION=`grep mod_release www/modules/$PROJECT/conf.php | cut -d '"' -f 4`

# Create source tarball.
git archive "--prefix=$PROJECT-$VERSION/" HEAD | gzip > "../input/$PROJECT-$VERSION.tar.gz"
cd ..

# Build RPMs.
cp `dirname $0`/../../packaging/dsm/$PROJECT-3.4.spectemplate input/
cp `dirname $0`/../../packaging/dsm/src/* input/
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-3.4:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-3.4:centos7 input output-centos7

# Copy files to server.
REPO="ubuntu@srvi-repo.int.centreon.com"
ssh "$REPO" mkdir -p "/srv/sources/lts/testing/$PROJECT-$VERSION-$RELEASE"
scp "input/$PROJECT-$VERSION.tar.gz" "$REPO:/srv/sources/lts/testing/$PROJECT-$VERSION-$RELEASE/"
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp $FILES_CENTOS6 "$REPO:/srv/yum/lts/3.4/el6/testing/noarch/RPMS"
scp $FILES_CENTOS7 "$REPO:/srv/yum/lts/3.4/el7/testing/noarch/RPMS"
ssh "$REPO" createrepo /srv/yum/lts/3.4/el6/testing/noarch
ssh "$REPO" createrepo /srv/yum/lts/3.4/el7/testing/noarch

# Generate testing documentation.
DOC="root@doc-dev.int.centreon.com"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-dsm -V latest -p'"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-dsm -V latest -p'"
