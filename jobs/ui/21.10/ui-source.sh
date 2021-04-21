#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

#
# This script will generate sources from the local clone of the project
# repository. These sources will then be pushed to the internal
# repository (srvi-repo) and used in downstream jobs, thanks to the
# property file generated at the end of the script.
#

# Project.
PROJECT=centreon-ui
tar czf "$PROJECT-git.tar.gz" "$PROJECT"

# Get version.
cd "$PROJECT"
export VERSION=$(cat package.json | grep version | head -1 | awk -F: '{ print $2 }' | sed 's/[",]//g' | tr -d '[[:space:]]')

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
export RELEASE="$now.$COMMIT"

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Create source tarballs (f*cking .gitattributes).
rm -rf "../$PROJECT-$VERSION"
mkdir "../$PROJECT-$VERSION"
git archive HEAD | tar -C "../$PROJECT-$VERSION" -x
cd ..
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

# Send it to srvi-repo.
put_internal_source "ui" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
