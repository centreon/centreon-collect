#!/bin/sh

. `dirname $0`/../common.sh

#
# This script will generate sources from the local clone of the project
# repository. These sources will then be pushed to oublic repository and used in downstream jobs, thanks to the
# property file generated at the end of the script.
#

# Project.
PROJECT=centreon-license-manager
tar czf "$PROJECT-git.tar.gz" "$PROJECT"

curl -o centreon-translations.php 'https://raw.githubusercontent.com/centreon/centreon/master/bin/centreon-translations.php'

# Get version.
cd "$PROJECT"
VERSION=`grep mod_release www/modules/$PROJECT/conf.php | cut -d '"' -f 4`
export VERSION="$VERSION"
MAJOR=`echo $VERSION | cut -d . -f 1,2`

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

# Generate lang file.
for i in "../$PROJECT-$VERSION/www/modules/centreon-license-manager/locale"/*.UTF-8 ; do
  lang=`basename $i | cut -d _ -f 1`
  msgfmt "$i/LC_MESSAGES/messages.po" -o "$i/LC_MESSAGES/messages.mo"
  php ../centreon-translations.php $lang "$i/LC_MESSAGES/messages.po" "$i/LC_MESSAGES/messages.ser"
  rm -f "$i/LC_MESSAGES/messages.po"
done

case $MAJOR in

  21.10)
    # Build frontend
    cd "../$PROJECT-$VERSION/www/modules/centreon-license-manager/frontend/"
    npm ci
    npm run build
    cd ..
    tar czf ../../../../frontend.tar.gz frontend
    rm -rf frontend
    cd ../../../..
    tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

    # Send it to srvi-repo
    curl -F "file=@$PROJECT-$VERSION.tar.gz" -F "version=80" 'https://encode.centreon.com/index.php' -o "$PROJECT-$VERSION-php80.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-php80.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "frontend.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"
    ;;

  21.04)
    # Build frontend
    cd "../$PROJECT-$VERSION/www/modules/centreon-license-manager/frontend/"
    npm ci
    npm run build
    cd ..
    tar czf ../../../../frontend.tar.gz frontend
    rm -rf frontend
    cd ../../../..
    tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

    # Send it to srvi-repo
    curl -F "file=@$PROJECT-$VERSION.tar.gz" -F "version=73" 'http://encode.int.centreon.com/api/index.php' -o "$PROJECT-$VERSION-php73.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-php73.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "frontend.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"
    ;;

  20.10)
    # Build frontend
    cd "../$PROJECT-$VERSION/www/modules/centreon-license-manager/frontend/"
    npm ci
    npm run build
    cd ..
    tar czf ../../../../frontend.tar.gz frontend
    rm -rf frontend
    cd ../../../..
    tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

    # Send it to srvi-repo
    curl -F "file=@$PROJECT-$VERSION.tar.gz" -F "version=72" 'http://encode.int.centreon.com/api/index.php' -o "$PROJECT-$VERSION-php72.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-php72.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" frontend.tar.gz
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"
    ;;

  20.04)
    # Setting the good node version
    sudo npm cache clean -f
    sudo npm install -g n
    sudo n 14

    # Build frontend
    cd "../$PROJECT-$VERSION/www/modules/centreon-license-manager/frontend/hooks"
    npm ci
    npm run build
    cd "../hooks/administration/extensions/manager/button"
    npm ci
    npm run build
    cd ../../../../..
    tar czf ../../../../../hooks.tar.gz hooks
    rm -rf hooks
    cd ../../../../..
    tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"

    # Send it to srvi-repo
    curl -F "file=@$PROJECT-$VERSION.tar.gz" -F "version=72" 'http://encode.int.centreon.com/api/index.php' -o "$PROJECT-$VERSION-php72.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-$VERSION-php72.tar.gz"
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" hooks.tar.gz
    put_internal_source "lm" "$PROJECT-$VERSION-$RELEASE" "$PROJECT-git.tar.gz"
    ;;

  *)
    echo -n "No build"
    exit 1
    ;;
esac

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
cp -r `dirname $0`/../common/build-artifacts summary
cp `dirname $0`/jobData.json summary/
generate_summary