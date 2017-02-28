#!/bin/sh

set -e
set -x

#
# This script will generate Centreon Engine sources from the local clone
# of Centreon Engine repository (centreon-engine directory). These
# sources will then be pushed to the internal repository (srvi-repo) and
# used in downstream jobs, thanks to the property file generated at the
# end of the script.
#

# Get version.
cd centreon-engine
cmakelists=build/CMakeLists.txt
major=`grep 'set(CENTREON_ENGINE_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_ENGINE_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_ENGINE_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$COMMIT"

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Create source tarball.
git archive --prefix="centreon-engine-$VERSION/" HEAD | gzip > "../centreon-engine-$VERSION.tar.gz"
cd ..

# Send it to srvi-repo.
FILES="centreon-engine-$VERSION.tar.gz"
DEST="/srv/sources/internal/centreon-engine-$VERSION-$RELEASE"
ssh -o StrictHostKeyChecking=no "ubuntu@srvi-repo.int.centreon.com" mkdir -p "$DEST"
scp -o StrictHostKeyChecking=no $FILES "ubuntu@srvi-repo.int.centreon.com:$DEST"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=centreon-engine
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
