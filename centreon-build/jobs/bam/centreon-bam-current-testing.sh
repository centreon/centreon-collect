#!/bin/sh


set -e
set -x

# Project.
PROJECT=centreon-bam-server

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ]; then
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

# Create source tarball.
cd centreon-bam
git checkout --detach "$COMMIT"
git archive --prefix="$PROJECT-$VERSION" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
cd ..

# Encrypt source tarballs.
curl -F file=@$PROJECT-$VERSION.tar.gz -F 'version=53' -F "modulename=$PROJECT" 'http://encode.int.centreon.com/api/' -o "$PROJECT-$VERSION-php53.tar.gz"
curl -F file=@$PROJECT-$VERSION.tar.gz -F 'version=54' -F "modulename=$PROJECT" 'http://encode.int.centreon.com/api/' -o "$PROJECT-$VERSION-php54.tar.gz"

# Copy spec file.
cp centreon-bam/packaging/*.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/bam/testing/$PROJECT-$VERSION-$RELEASE"
scp -o StrictHostKeyChecking=no "input/centreon-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE/"
scp -o StrictHostKeyChecking=no "input/centreon-web-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE/"
scp -o StrictHostKeyChecking=no "$PROJECT-$VERSION-php5{3,4}.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/bam/testing/$PROJECT-$VERSION-$RELEASE"
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/bam/3.4/el6/testing/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/bam/3.4/el7/testing/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/bam/3.4/el6/testing/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/bam/3.4/el7/testing/noarch

# Generate testing documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-bam -V 3.5.x -p'"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-bam -V 3.5.x -p'"
