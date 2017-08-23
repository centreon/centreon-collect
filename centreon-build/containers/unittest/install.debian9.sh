#!/bin/sh

set -e
set -x

# Update repository information.
apt-get update

# Install build dependencies.
xargs apt-get install < /tmp/build-dependencies.txt

# Install PHPunit and Xdebug (for PHPunit code coverage).
apt-get install phpunit php-xdebug

# Install Composer.
apt-get install php php-cli php-curl php-mbstring php-xml
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer
