#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs apt-get install < /tmp/build-dependencies.txt

# Install PHPunit and Xdebug (for PHPunit code coverage).
apt-get install phpunit php-xdebug

# Install Composer.
apt-get install php php-cli
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer
