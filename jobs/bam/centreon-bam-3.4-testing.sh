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
cd centreon-bam
git checkout --detach "$COMMIT"
export VERSION=`grep mod_release www/modules/centreon-bam-server/conf.php | cut -d '"' -f 4`

# Create source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
cd ..

# Encrypt source tarballs.
curl -F file=@$PROJECT-$VERSION.tar.gz -F 'version=53' -F "modulename=$PROJECT" 'http://encode.int.centreon.com/api/' -o "input/$PROJECT-$VERSION-php53.tar.gz"
curl -F file=@$PROJECT-$VERSION.tar.gz -F 'version=54' -F "modulename=$PROJECT" 'http://encode.int.centreon.com/api/' -o "input/$PROJECT-$VERSION-php54.tar.gz"

# Copy spec file.
cp centreon-bam/packaging/*.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-3.4:centos6 input output-centos6
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-3.4:centos7 input output-centos7

# Copy files to server.
ssh "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/bam/testing/$PROJECT-$VERSION-$RELEASE"
scp "$PROJECT-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/bam/testing/$PROJECT-$VERSION-$RELEASE/"
scp "input/$PROJECT-$VERSION-php53.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/bam/testing/$PROJECT-$VERSION-$RELEASE/"
scp "input/$PROJECT-$VERSION-php54.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/bam/testing/$PROJECT-$VERSION-$RELEASE/"
FILES_CENTOS6='output-centos6/noarch/*.rpm'
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS6 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/bam/3.4/el6/testing/noarch/RPMS"
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/bam/3.4/el7/testing/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/bam/3.4/el6/testing/noarch
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/bam/3.4/el7/testing/noarch
