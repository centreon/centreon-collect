#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-bam-server

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ]; then
  echo "You need to specify COMMIT and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies-18.10:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
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
curl -F file=@$PROJECT-$VERSION.tar.gz -F 'version=71' -F "modulename=$PROJECT" 'http://encode.int.centreon.com/api/' -o "input/$PROJECT-$VERSION-php71.tar.gz"

# Copy spec file.
cp centreon-bam/packaging/*.spectemplate input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-18.10:centos7 input output-centos7

# Copy files to server.
put_testing_source "bam" "bam" "$PROJECT-$VERSION-$RELEASE" "input/$PROJECT-$VERSION-php71.tar.gz"
put_testing_rpms "bam" "18.10" "el7" "noarch" "bam" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm

# Generate testing documentation.
SSH_DOC="$SSH_REPO ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-bam -V latest -p'"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-bam -V latest -p'"
