#!/bin/sh

set -e
set -x

. `dirname $0`/../../common.sh

# Project.
PROJECT=centreon-mbi

# Get version.
cd "$PROJECT"
VERSION=`grep mod_release server/www/modules/centreon-bi-server/conf.php | cut -d '"' -f 4`
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

#
# ENGINE / ETL / REPORT / REPORTING-SERVER
#
for i in engine etl report reporting-server ; do
  cd "$i"
  git archive --prefix="centreon-bi-$i-$VERSION/" HEAD . | gzip > "../../centreon-bi-$i-$VERSION.tar.gz"
  cd ..
done

#
# SERVER
#
rm -rf "../centreon-bi-server-$VERSION"
mkdir "../centreon-bi-server-$VERSION"
cd server
curl -o centreon-translations.php 'https://raw.githubusercontent.com/centreon/centreon/master/bin/centreon-translations.php'
git archive HEAD . | tar -C "../../centreon-bi-server-$VERSION" -x
for i in "../../centreon-bi-server-$VERSION/www/modules/centreon-bi-server/locale"/*.UTF-8 ; do
  lang=`basename $i | cut -d _ -f 1`
  msgfmt "$i/LC_MESSAGES/messages.po" -o "$i/LC_MESSAGES/messages.mo"
  php centreon-translations.php $lang "$i/LC_MESSAGES/messages.po" "$i/LC_MESSAGES/messages.ser"
  rm -f "$i/LC_MESSAGES/messages.po"
done
cd ../..
tar czf "centreon-bi-server-$VERSION.tar.gz" "centreon-bi-server-$VERSION"

# Send sources to srvi-repo.
curl -F "file=@centreon-bi-server-$VERSION.tar.gz" -F "version=71" 'http://encode.int.centreon.com/api/index.php' -o "centreon-bi-server-$VERSION-php71.tar.gz"
for i in engine etl report reporting-server ; do
  put_internal_source "mbi" "$PROJECT-$VERSION-$RELEASE" "centreon-bi-$i-$VERSION.tar.gz"
done
put_internal_source "mbi" "$PROJECT-$VERSION-$RELEASE" "centreon-bi-server-$VERSION-php71.tar.gz"
put_internal_source "mbi" "$PROJECT-$VERSION-$RELEASE" "$PROJECT/server/packaging/centreon-bi-server.spectemplate"

# Generate properties files for downstream jobs.
cat > source.properties << EOF
PROJECT=$PROJECT
VERSION=$VERSION
RELEASE=$RELEASE
COMMIT=$COMMIT
COMMITTER=$COMMITTER
EOF
