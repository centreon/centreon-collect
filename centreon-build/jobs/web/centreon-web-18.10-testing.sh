#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

# Project.
PROJECT=centreon-web

# Check arguments.
if [ -z "$COMMIT" -o -z "$VERSION" -o -z "$RELEASE" ] ; then
  echo "You need to specify COMMIT, VERSION and RELEASE environment variables."
  exit 1
fi

# Pull mon-build-dependencies containers.
BUILD_CENTOS7=registry.centreon.com/mon-build-dependencies-18.10:centos7
docker pull "$BUILD_CENTOS7"

# Prepare base source tarball.
cd "$PROJECT"
git checkout --detach "$COMMIT"
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
SRCCOMMIT=`git log -1 HEAD --pretty=format:%h`
cd ..

# Create and populate container.
containerid=`docker create -e "PROJECT=$PROJECT" -e "VERSION=$VERSION" -e "COMMIT=$SRCCOMMIT" $BUILD_CENTOS7 /usr/local/bin/source.sh`
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

# Retrieve spec file and additional sources.
cp `dirname $0`/../../packaging/web/rpm/18.10/centreon.spectemplate input/
cp `dirname $0`/../../packaging/web/src/18.10/* input/
cp "centreon-$VERSION.tar.gz" input/

# Build RPMs.
docker-rpm-builder dir --sign-with `dirname $0`/../ces.key "$BUILD_CENTOS7" input output-centos7

# Copy files to server.
put_testing_source "standard" "web" "$PROJECT-$VERSION-$RELEASE" "centreon-$VERSION.tar.gz" "$PROJECT-$VERSION.tar.gz"
put_testing_rpms "standard" "18.10" "el7" "noarch" "web" "$PROJECT-$VERSION-$RELEASE" output-centos7/noarch/*.rpm
