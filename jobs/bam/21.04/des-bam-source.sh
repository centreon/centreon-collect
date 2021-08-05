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
PROJECT=centreon-bam-server
tar czf "$PROJECT-git.tar.gz" "$PROJECT"
curl -o centreon-translations.php 'https://raw.githubusercontent.com/centreon/centreon/master/bin/centreon-translations.php'

# Get version.
cd "$PROJECT"
VERSION=`grep mod_release www/modules/$PROJECT/conf.php | head -n 1 | cut -d '"' -f 4`
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

sudo npm cache clean -f
sudo npm install -g n
sudo n latest

# Create source tarballs (f*cking .gitattributes).
rm -rf "../$PROJECT-$VERSION"
mkdir "../$PROJECT-$VERSION"
git archive HEAD | tar -C "../$PROJECT-$VERSION" -x
for i in "../$PROJECT-$VERSION/www/modules/centreon-bam-server/locale"/*.UTF-8 ; do
  lang=`basename $i | cut -d _ -f 1`
  msgfmt "$i/LC_MESSAGES/messages.po" -o "$i/LC_MESSAGES/messages.mo"
  php ../centreon-translations.php $lang "$i/LC_MESSAGES/messages.po" "$i/LC_MESSAGES/messages.ser"
  rm -f "$i/LC_MESSAGES/messages.po"
done
cd "../$PROJECT-$VERSION/www/modules/centreon-bam-server/react"
OLDDIR=`pwd`
find . -name package.json | xargs dirname > reacttargets.txt
for i in `cat reacttargets.txt` ; do
  cd "$OLDDIR/$i"
  npm ci
  npm run build
done
cd "$OLDDIR/../../../../.."
rm -rf "$PROJECT-$VERSION/www/modules/centreon-bam-server/react"
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
cd "$PROJECT"
echo '/* -export-ignore' > .git/info/attributes
git archive --prefix="$PROJECT-$VERSION-full/" HEAD | gzip > "../$PROJECT-$VERSION-full.tar.gz"
rm -f .git/info/attributes
cd ..

# Send it to srvi-repo.
curl -F "file=@$PROJECT-$VERSION.tar.gz" -F "version=73" 'http://encode.int.centreon.com/api/index.php' -o "$PROJECT-$VERSION-php73.tar.gz"
put_internal_source "bam" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
put_internal_source "bam" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-full.tar.gz"
put_internal_source "bam" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-php73.tar.gz"
put_internal_source "bam" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"

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
