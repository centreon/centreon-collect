#!/bin/sh

set -e
set -x

# Prepare source directory.
cd /usr/local/src
tar xzf "$PROJECT-$VERSION.tar.gz"

# Replace basic macros.
cd "$PROJECT-$VERSION"
find . -type f | xargs --delimiter='\n' sed -i -e "s/@COMMIT@/$COMMIT/g"

# Generate release notes.
major=`echo $VERSION | cut -d . -f 1`
minor=`echo $VERSION | cut -d . -f 2`
cd "doc/en/release_notes"
cp ../Makefile .
cp ../conf.py .
make SPHINXOPTS="-D html_theme=scrolls" html
cp "_build/html/centreon-$major.$minor.html" "../../../www/install/RELEASENOTES.html"
sed -i \
    -e "/<link/d" \
    -e "/<script .*>.*<\/script>/d" \
    -e "s/href=\"..\//href=\"http:\/\/documentation.centreon.com\/docs\/centreon\/en\/latest\//g" \
    -e "/<\/head>/i \
    <style type=\"text/css\">\n \
    #toc, .footer, .relnav, .header { display: none; }\n \
    <\/style>" ../../../www/install/RELEASENOTES.html
make clean
rm -rf Makefile conf.py _build

# Generate lang files.
cd ../../..
# Special case for english front-end translation that uses french as base.
mkdir -p www/locale/en_US.UTF-8/LC_MESSAGES
php bin/centreon-translations.php en lang/fr_FR.UTF-8/LC_MESSAGES/messages.po www/locale/en_US.UTF-8/LC_MESSAGES/messages.ser
for i in lang/* ; do
  localefull=`basename $i`
  langcode=`echo $localefull | cut -d _ -f 1`
  mkdir -p "www/locale/$localefull/LC_MESSAGES"
  msgfmt "lang/$localefull/LC_MESSAGES/messages.po" -o "www/locale/$localefull/LC_MESSAGES/messages.mo"
  msgfmt "lang/$localefull/LC_MESSAGES/help.po" -o "www/locale/$localefull/LC_MESSAGES/help.mo"
  php bin/centreon-translations.php "$langcode" "lang/$localefull/LC_MESSAGES/messages.po" "www/locale/$localefull/LC_MESSAGES/messages.ser"
done
rm -rf lang

# Generate API documentation.
redoc-cli bundle --options.hideDownloadButton=true doc/API/centreon-api-v2.yaml -o ../centreon-api-v2.html

# Install Composer dependencies.
composer install --no-dev --optimize-autoloader

# Install npm dependencies.
npm ci
npm run build
rm -rf node_modules

# Create source tarballs.
cd ..
rm -f "$PROJECT-$VERSION.tar.gz"
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
cd "$PROJECT-$VERSION"
composer install
rm -f "../vendor.tar.gz"
tar czf "../vendor.tar.gz" "vendor"
