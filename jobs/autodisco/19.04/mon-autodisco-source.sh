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
PROJECT=centreon-autodiscovery

# Get version.
cd "$PROJECT"
VERSION=`grep mod_release www/modules/centreon-autodiscovery-server/conf.php | cut -d '"' -f 4`
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
rm -rf "../$PROJECT-$VERSION"
mkdir "../$PROJECT-$VERSION"
git archive HEAD | tar -C "../$PROJECT-$VERSION" -x
# build job listing page
cd "../$PROJECT-$VERSION/www/modules/centreon-autodiscovery-server/react/pages/configuration/hosts/discovery/jobs"
npm ci
npm run build
# build discovered hosts listing page
cd _id
npm ci
npm run build
# clean frontend sources
cd ../../../../../../..
rm -rf react
# archive module
cd ../../../..
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

# Send it to srvi-repo.
curl -F "file=@$PROJECT-$VERSION.tar.gz" -F "version=71" 'http://encode.int.centreon.com/api/index.php' -o "$PROJECT-$VERSION-php71.tar.gz"
put_internal_source "autodisco" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
put_internal_source "autodisco" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-php71.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
