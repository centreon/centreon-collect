#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Check arguments.
if [ -z "$WIDGET" ] ; then
  echo "You need to specify WIDGET environment variable."
  exit 1
fi

# Project.
export PROJECT=centreon-widget-$WIDGET
tar czf "$PROJECT-git.tar.gz" "$PROJECT"

# Get release.
cd $PROJECT
export VERSION="`sed -n 's|\s*<version>\(.*\)</version>|\1|p' $WIDGET/configs.xml 2>/dev/null`"
export SUMMARY="`sed -n 's|\s*<description>\(.*\)</description>|\1|p' $WIDGET/configs.xml 2>/dev/null`"
if [ "$BUILD" '=' 'RELEASE' ] ; then
  export RELEASE="$BUILD_NUMBER"
else
  now=`date +%s`
  COMMIT=`git log -1 HEAD --pretty=format:%h`
  export RELEASE="$now.$COMMIT"
fi

# Get committer.
COMMITTER=`git show --format='%cN <%cE>' HEAD | head -n 1`

# Prepare base source tarball.
rm -rf "../$PROJECT-$VERSION"
mkdir "../$PROJECT-$VERSION"
git archive HEAD | tar -C "../$PROJECT-$VERSION" -x
cd "../$PROJECT-$VERSION"

# Install npm dependencies and build widget.
if [ -f "package.json" ]; then
  npm ci
  npm run build
  rm -rf node_modules
fi

# Generate source tarball.
cd ..
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

# Send it to srvi-repo.
put_internal_source "widgets" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
WIDGET=$WIDGET
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
