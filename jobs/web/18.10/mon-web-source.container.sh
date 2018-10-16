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
cd "doc/en"
make SPHINXOPTS="-D html_theme=scrolls" html
cp "_build/html/release_notes/centreon-$major.$minor/centreon-$VERSION.html" "../../www/install/RELEASENOTES.html"
sed -i \
    -e "/<link/d" \
    -e "/<script .*>.*<\/script>/d" \
    -e "s/href=\"..\//href=\"http:\/\/documentation.centreon.com\/docs\/centreon\/en\/latest\//g" \
    -e "/<\/head>/i \
    <style type=\"text/css\">\n \
    #toc, .footer, .relnav, .header { display: none; }\n \
    <\/style>" ../../www/install/RELEASENOTES.html
make clean

# Generate lang files.
cd ../..
# es_ES
mkdir -p www/locale/es_ES.UTF-8/LC_MESSAGES
msgfmt lang/es/LC_MESSAGES/messages.po -o www/locale/es_ES.UTF-8/LC_MESSAGES/messages.mo
msgfmt lang/es/LC_MESSAGES/help.po -o www/locale/es_ES.UTF-8/LC_MESSAGES/help.mo
# fr_FR
mkdir -p www/locale/fr_FR.UTF-8/LC_MESSAGES
msgfmt lang/fr/LC_MESSAGES/messages.po -o www/locale/fr_FR.UTF-8/LC_MESSAGES/messages.mo
msgfmt lang/fr/LC_MESSAGES/help.po -o www/locale/fr_FR.UTF-8/LC_MESSAGES/help.mo
# pt_BR
mkdir -p www/locale/pt_BR.UTF-8/LC_MESSAGES
msgfmt lang/pt_BR/LC_MESSAGES/messages.po -o www/locale/pt_BR.UTF-8/LC_MESSAGES/messages.mo
msgfmt lang/pt_BR/LC_MESSAGES/help.po -o www/locale/pt_BR.UTF-8/LC_MESSAGES/help.mo
# pt_PT
mkdir -p www/locale/pt_PT.UTF-8/LC_MESSAGES
msgfmt lang/pt_PT/LC_MESSAGES/messages.po -o www/locale/pt_PT.UTF-8/LC_MESSAGES/messages.mo
msgfmt lang/pt_PT/LC_MESSAGES/help.po -o www/locale/pt_PT.UTF-8/LC_MESSAGES/help.mo
rm -rf lang

# Install Composer dependencies.
composer install --no-dev --optimize-autoloader

# Install npm dependencies.
npm install
npm run build
rm -rf node_modules

# Create source tarballs.
cd ..
rm -f "$PROJECT-$VERSION.tar.gz"
tar czf "$PROJECT-$VERSION.tar.gz" "$PROJECT-$VERSION"
