#!/bin/sh

. `dirname $0`/../../common.sh

#
# This script will generate sources from the local clone of the project
# repository. These sources will then be pushed to the internal
# repository (srvi-repo) and used in downstream jobs, thanks to the
# property file generated at the end of the script.
#

# Project.
PROJECT=centreon-bam-server

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

cd ..
tar czf "$PROJECT-$VERSION-git.tar.gz" "$PROJECT-$VERSION"

# temporary fix to solve the beberlei/assert requirement, needed for acceptation tests
# @TODO build a container with these packages to be able to execute tests on it directly
#sudo apt-get update
#sudo apt-get install -y php-intl
cd "$PROJECT-$VERSION"
composer install --ignore-platform-reqs
rm -rf ../vendor
mv vendor ../
cd ..
tar czf "vendor.tar.gz" "vendor"

cd "$PROJECT-$VERSION/www/modules/centreon-bam-server/react"
npm ci
npm run build
cd "../../../../.."
rm -rf node_modules
mv "$PROJECT-$VERSION/www/modules/centreon-bam-server/react/node_modules" ./
tar czf "node_modules.tar.gz" "node_modules"
rm -rf "$PROJECT-$VERSION/www/modules/centreon-bam-server/react"
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

# Send it to srvi-repo.
curl -F "file=@$PROJECT-$VERSION.tar.gz" -F "version=80" 'https://encode.centreon.com/index.php' -o "$PROJECT-$VERSION-php80.tar.gz"

mv "$PROJECT-$VERSION-git.tar.gz" "$PROJECT-$VERSION.tar.gz"

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
