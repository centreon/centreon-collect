#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-connector
tar czf "$PROJECT-git.tar.gz" "$PROJECT"

#
# This script will generate Centreon Connector sources from the local clone
# of Centreon Connector repository (centreon-connector directory). These
# sources will then be pushed to the internal repository (srvi-repo) and
# used in downstream jobs, thanks to the property file generated at the
# end of the script.
#

# Get version.
cd $PROJECT
cmakelists=perl/build/CMakeLists.txt
major=`grep 'set(CONNECTOR_PERL_MAJOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
minor=`grep 'set(CONNECTOR_PERL_MINOR' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
patch=`grep 'set(CONNECTOR_PERL_PATCH' "$cmakelists" | cut -d ' ' -f 2 | cut -d ')' -f 1`
export VERSION="$major.$minor.$patch"

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
git archive --prefix="centreon-connectors-$VERSION/" HEAD | gzip > "../centreon-connectors-$VERSION.tar.gz"
cd ..

# Send it to srvi-repo.
put_internal_source "connector" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
put_internal_source "connector" "$PROJECT-$VERSION-$RELEASE" "centreon-connectors-$VERSION.tar.gz"
put_internal_source "connector" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=centreon-connector
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
