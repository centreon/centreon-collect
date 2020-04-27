#!/bin/sh

set -e
set -x

. `dirname $0`/../common.sh

#
# This script will generate sources from the local clone of the project
# repository. These sources will then be pushed to the internal
# repository (srvi-repo) and used in downstream jobs, thanks to the
# property file generated at the end of the script.
#

# Project.
PROJECT=centreon-plugins

# Get version.
export VERSION=`date '+%Y%m%d'`

# Get release.
export RELEASE=`date '+%H%M%S'`

# Get committer.
cd "$PROJECT"
COMMIT=`git log -1 HEAD --pretty=format:%h`
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`
cd ..

# Create and populate container.
IMAGE="registry.centreon.com/mon-build-dependencies-19.04:centos7"
docker pull "$IMAGE"
containerid=`docker create $IMAGE /usr/local/bin/source.pl "$VERSION ($COMMIT)"`
docker cp `dirname $0`/plugins-source.container.pl "$containerid:/usr/local/bin/source.pl"
docker cp "$PROJECT" "$containerid:/usr/local/src/$PROJECT"
docker cp `dirname $0`/../../packaging/plugins "$containerid:/usr/local/src/packaging-$PROJECT"

# Run container to fatpack all plugins.
docker start -a "$containerid"
rm -rf "build"
docker cp "$containerid:/usr/local/src/build" "build"

# Stop container.
docker stop "$containerid"
docker rm "$containerid"

# Create source tarball.
rm -rf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
mv build "$PROJECT-$VERSION"
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

# Send it to srvi-repo.
put_internal_source "plugins" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
