#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

if [ "$#" -lt 1 ] ; then
  echo "USAGE: $0 <widget_name>"
  exit 1
fi
WIDGET="$1"

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
git archive --prefix="$PROJECT-$VERSION/" HEAD | gzip > "../$PROJECT-$VERSION.tar.gz"
cd ..

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
