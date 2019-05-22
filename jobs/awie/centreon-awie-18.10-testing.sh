#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-awie

# Check arguments.
if [ -z "$COMMIT" -o -z "$RELEASE" ] ; then
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
cd "$PROJECT"
git checkout --detach "$COMMIT"
export VERSION=`grep mod_release www/modules/centreon-awie/conf.php | cut -d '"' -f 4`

# Create source tarball.
git archive "--prefix=$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
cd ..

cp $PROJECT-$VERSION.tar.gz input/$PROJECT-$VERSION.tar.gz

# Build RPMs.
cp $PROJECT/packaging/* input/
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-18.10:centos7 input output-centos7

# Copy files to server.
put_testing_source "standard" "awie" "$PROJECT-$VERSION-$RELEASE" "input/$PROJECT-$VERSION.tar.gz"
put_testing_rpms "standard" "18.10" "el7" "noarch" "awie" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm

# Create entry in download-dev.
SRCHASH=00112233445566778899aabbccddeeff
curl "$DLDEV_URL/api/?token=ML2OA4P43FDF456FG3EREYUIBAHT521&product=$PROJECT&version=$VERSION&extension=tar.gz&md5=$SRCHASH&ddos=0&dryrun=0"

# Generate testing documentation.
DOC="root@doc-dev.int.centreon.com"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-awie -V latest -p'"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-awie -V latest -p'"
