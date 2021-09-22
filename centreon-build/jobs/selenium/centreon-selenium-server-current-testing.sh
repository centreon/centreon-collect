#!/bin/sh

set -e
set -x

# Project.
NAME=centreon-selenium-server

# Check arguments.
if [ -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify VERSION, RELEASE, NAME environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
docker pull registry.centreon.com/mon-build-dependencies:centos7

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Create source tarball.
rm -rf $NAME
git clone https://github.com/centreon/centreon-web-application-analytics.git $NAME
cd $NAME
git checkout --detach "tags/$VERSION"
rm -rf "../$NAME-$VERSION"
mkdir "../$NAME-$VERSION"
git archive HEAD | tar -C "../$NAME-$VERSION" -x
cd ..
tar czf "input/$NAME-$VERSION.tar.gz" "$NAME-$VERSION"

# Build RPMs.
cp `dirname $0`/../../packaging/selenium/$NAME.spectemplate input/
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key registry.centreon.com/mon-build-dependencies:centos7 input output-centos7

# Copy files to server.
REPO="ubuntu@srvi-repo.int.centreon.com"
ssh "$REPO" mkdir -p "/srv/sources/standard/testing/$NAME-$VERSION-$RELEASE"
scp "input/$NAME-$VERSION.tar.gz" "$REPO:/srv/sources/standard/testing/$NAME-$VERSION-$RELEASE/"
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp $FILES_CENTOS7 "$REPO:/srv/yum/standard/3.4/el7/testing/noarch/RPMS"
ssh "$REPO" createrepo /srv/yum/standard/3.4/el7/testing/noarch

# Generate testing documentation.
DOC="root@doc-dev.int.centreon.com"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon-web-application-analytics -V latest -p'"
ssh "$DOC" bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon-web-application-analytics -V latest -p'"
