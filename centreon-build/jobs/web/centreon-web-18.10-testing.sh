#!/bin/sh

set -e
set -x

# Project.
PROJECT=centreon-web

# Check arguments.
if [ -z "$COMMIT" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT, VERSION and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
BUILD_CENTOS7=ci.int.centreon.com:5000/mon-build-dependencies-18.10:centos7
docker pull "$BUILD_CENTOS7"

# Prepare base source tarball.
cd "$PROJECT"
git checkout --detach "$COMMIT"
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
SRCCOMMIT=`git log -1 HEAD --pretty=format:%h`
cd ..

# Create and populate container.
containerid=`docker create -e "PROJECT=$PROJECT" -e "VERSION=$VERSION" -e "COMMIT=$SRCCOMMIT" $BUILD_CENTOS7 /usr/local/bin/source`
docker cp `dirname $0`/18.10/mon-web-source.container.sh "$containerid:/usr/local/bin/source.sh"
docker cp "$PROJECT-$VERSION.tar.gz" "$containerid:/usr/local/src/"

# Run container that will generate complete tarball.
docker start -a "$containerid"
rm -f "$PROJECT-$VERSION.tar.gz"
docker cp "$containerid:/usr/local/src/$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION.tar.gz"
rm -rf "$PROJECT-$VERSION" "centreon-$VERSION" "centreon-$VERSION.tar.gz"
tar xzf "$PROJECT-$VERSION.tar.gz"
mv "$PROJECT-$VERSION" "centreon-$VERSION"
tar czf "centreon-$VERSION.tar.gz" "centreon-$VERSION"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Create input and output directories for docker-rpm-builder.
rm -rf input
mkdir input
rm -rf output-centos7
mkdir output-centos7

# Retrieve spec file.
rm -rf packaging-centreon-web
git clone http://gitbot:gitbot@git.int.centreon.com/packaging-centreon packaging-centreon-web
cp packaging-centreon-web/rpm/centreon-18.10.spectemplate input/

# Retrieve additional sources.
cp packaging-centreon-web/src/18.10/* input/
cp "centreon-$VERSION.tar.gz" input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input output-centos7

# Copy files to server.
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE"
scp -o StrictHostKeyChecking=no "centreon-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE/"
scp -o StrictHostKeyChecking=no "$PROJECT-$VERSION.tar.gz" "ubuntu@srvi-repo.int.centreon.com:/srv/sources/standard/testing/$PROJECT-$VERSION-$RELEASE/"
FILES_CENTOS7='output-centos7/noarch/*.rpm'
scp -o StrictHostKeyChecking=no $FILES_CENTOS7 "ubuntu@srvi-repo.int.centreon.com:/srv/yum/standard/18.10/el7/testing/noarch/RPMS"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" createrepo /srv/yum/standard/18.10/el7/testing/noarch

# Generate doc.
SSH_DOC="ssh -o StrictHostKeyChecking=no root@doc-dev.int.centreon.com"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage.py update_repos centreon -V latest -p'"
$SSH_DOC bash -c "'source /srv/env/documentation/bin/activate ; /srv/prod/readthedocs.org/readthedocs/manage_fr.py update_repos centreon -V latest -p'"
