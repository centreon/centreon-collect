#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

#
# This script will generate Centreon Discovery Engine sources from the
# local clone of Centreon Discovery Engine repository
# (centreon-discovery-engine directory). These sources will then be
# pushed to the internal repository (srvi-repo) and used in downstream
# jobs, thanks to the property file generated at the end of the script.
#

# Project.
PROJECT=centreon-discovery-engine

# Get version.
cd $PROJECT
cmakelists=build/CMakeLists.txt
major=`grep 'set(CENTREON_DISCOVERY_ENGINE_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_DISCOVERY_ENGINE_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_DISCOVERY_ENGINE_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$COMMIT"

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Create source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
cd ..

# Send it to srvi-repo.
put_internal_source "automation-broker" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=centreon-broker
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
