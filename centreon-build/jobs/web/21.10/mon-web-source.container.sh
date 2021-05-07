#!/bin/sh

set -e
set -x

# Prepare source directory.
cd /usr/local/src
tar xzf "$PROJECT-$VERSION.tar.gz"

# restore or install dependencies
cd "$PROJECT-$VERSION"
if [ -f "../node_modules.tar.gz" ]; then
  tar xzf "../node_modules.tar.gz" -C "./"
else
  npm ci
  tar zcf "../node_modules.tar.gz" node_modules
fi
if [ -f "../vendor.tar.gz" ]; then
  tar xzf "../vendor.tar.gz" -C "./"
else
  composer install
  tar zcf "../vendor.tar.gz" vendor
fi

# Replace basic macros.
find ./www/include/Administration/about -type f | xargs --delimiter='\n' sed -i -e "s/@COMMIT@/$COMMIT/g"

# Generate lang files.
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

# Create source tarballs.
cd ..
rm -f "$PROJECT-$VERSION.tar.gz"
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
