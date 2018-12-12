#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-open-tickets

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
export VERSION=`grep mod_release www/modules/centreon-open-tickets/conf.php | cut -d '"' -f 4`

# Create source tarball.
git archive "--prefix=$PROJECT-$VERSION/" HEAD | gzip > "../input/$PROJECT-$VERSION.tar.gz"
cd ..

# Build RPMs.
cp `dirname $0`/../../packaging/open-tickets/centreon-open-tickets-18.10.spectemplate input/
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies-18.10:centos7 input output-centos7

# Copy files to server.
put_testing_source "standard" "open-tickets" "$PROJECT-$VERSION-$RELEASE" "input/$PROJECT-$VERSION.tar.gz"
put_testing_rpms "standard" "18.10" "el7" "noarch" "open-tickets" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm

# Generate testing documentation.
DOC="root@doc-dev.int.centreon.com"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-open-tickets -V latest -p'"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-open-tickets -V latest -p'"
