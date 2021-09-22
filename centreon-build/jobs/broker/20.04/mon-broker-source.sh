#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

#
# This script will generate Centreon Broker sources from the local clone
# of Centreon Broker repository (centreon-broker directory). These
# sources will then be pushed to the internal repository (srvi-repo) and
# used in downstream jobs, thanks to the property file generated at the
# end of the script.
#

# Project.
PROJECT=centreon-broker
tar czf "$PROJECT-git.tar.gz" "$PROJECT"

# Get version.
cd $PROJECT
cmakelists=CMakeLists.txt
major=`grep 'set(CENTREON_BROKER_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CENTREON_BROKER_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CENTREON_BROKER_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
prerelease=`grep 'set(CENTREON_BROKER_PRERELEASE' "$cmakelists" | cut -d '"' -f 2 || true`
if [ -n "$prerelease" ] ; then
  export VERSION="$major.$minor.$patch-$prerelease"
else
  export VERSION="$major.$minor.$patch"
fi

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
put_internal_source "broker" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
put_internal_source "broker" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=centreon-broker
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
