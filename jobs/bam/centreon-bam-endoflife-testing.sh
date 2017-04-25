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

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output
mkdir output

# Get version.
cd centreon-bam
git checkout --detach "$COMMIT"
export VERSION=`grep mod_release www/modules/centreon-bam-server/conf.php | cut -d '"' -f 4`

# Create source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
cd ..

# Encrypt source tarballs.
curl -F file=@$PROJECT-$VERSION.tar.gz -F 'version=53' -F "modulename=$PROJECT" 'http://encode.int.centreon.com/api/' -o "input/$PROJECT-$VERSION-php53.tar.gz"

# Copy spec file.
cp centreon-bam/packaging/*.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key ci.int.centreon.com:5000/mon-build-dependencies:centos6 input output

# Copy files to server.
ssh "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/bam/testing/$PROJECT-$VERSION-$RELEASE"
scp "$PROJECT-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/bam/testing/$PROJECT-$VERSION-$RELEASE/"
scp "input/$PROJECT-$VERSION-php53.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/bam/testing/$PROJECT-$VERSION-$RELEASE/"
FILES='output/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES "ubuntu@srvi-repo.int.centreon.com:/srv/yum/bam/2/el6/testing/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/bam/2/el6/testing

# Generate testing documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-bam -V 3.4.x -p'"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-bam -V 3.4.x -p'"
