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
PROJECT=centreon-ha
tar czf "$PROJECT-git.tar.gz" "$PROJECT"

# Get version.
cd "$PROJECT"
VERSION='20.04.0'
export VERSION="$VERSION"

# Get release.
COMMIT=`git log -1 HEAD --pretty=format:%h`
now=`date +%s`
if [ "$BUILD" '=' 'RELEASE' ] ; then
  export RELEASE="$BUILD_NUMBER"
else
  export RELEASE="$now.$COMMIT"
fi

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Create source tarball.
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
cd ..

# Send it to srvi-repo.
put_internal_source "ha" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
put_internal_source "ha" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF

# Generate summary report.
rm -rf summary
cp -r `dirname $0`/../../common/build-artifacts summary
cp `dirname $0`/jobData.json summary/
generate_summary
