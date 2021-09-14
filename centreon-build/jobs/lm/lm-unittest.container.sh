#!/bin/sh

# Get project.
PROJECT=centreon-license-manager

# Remove old reports.
rm -f /tmp/ut-*.xml
rm -f /tmp/coverage.xml
rm -f /tmp/codestyle-*.xml

# Install dependencies.
cd /usr/local/src/$PROJECT/
# @TODO remove credentials
 
composer install 

# Prepare build directory
mkdir -p build
MAJOR=`echo $VERSION | cut -d . -f 1,2`

# Run backend unit tests and code style.
if [ "$MAJOR" = "20.10" -o "$MAJOR" = "20.04" ] ; then
    XDEBUG_MODE=coverage ./vendor/bin/phing unittest
    ./vendor/bin/phing codestyle
else
    XDEBUG_MODE=coverage composer run-script test:ci
    composer run-script codestyle:ci
    composer run-script phpstan:ci > build/phpstan.xml
fi

# Move reports to expected places.
mv build/phpunit.xml /tmp/ut-be.xml
mv build/coverage.xml /tmp/coverage.xml
mv build/checkstyle.xml /tmp/codestyle-be.xml
if [ "$MAJOR" != "20.10" -o "$MAJOR" != "20.04" ] ; then
    mv build/phpstan.xml /tmp/phpstan.xml
fi

if [ "$MAJOR" = "20.04" ] ; then
    cd "www/modules/centreon-license-manager/hooks"
else
    cd "www/modules/centreon-license-manager/frontend"
fi

# Run frontend unit tests and code style.
npm run eslint -- -o checkstyle-fe.xml -f checkstyle
npm t -- --ci --reporters=jest-junit

# Move reports to expected places.
mv junit.xml /tmp/ut-fe.xml
mv checkstyle-fe.xml /tmp/codestyle-fe.xml
