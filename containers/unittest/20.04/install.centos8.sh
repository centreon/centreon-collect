#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs dnf install < /tmp/build-dependencies.txt

# Install PHPUnit.
curl -o /usr/local/bin/phpunit https://phar.phpunit.de/phpunit-8.phar
chmod +x /usr/local/bin/phpunit

# Install Composer.
dnf install -y php php-cli php-dom php-mbstring
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer
