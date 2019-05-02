#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs yum install < /tmp/build-dependencies.txt

# Install PHPUnit (the 7.x release being the latest to support PHP 7.1).
curl -o /usr/local/bin/phpunit https://phar.phpunit.de/phpunit-7.phar
chmod +x /usr/local/bin/phpunit

# Install Xdebug PHP extension (for PHPUnit code coverage).
yum install sclo-php71-php-pecl-xdebug

# Install Composer.
yum install -y rh-php71-php rh-php71-php-cli rh-php71-php-dom rh-php71-php-mbstring
export PATH="$PATH:/opt/rh/rh-php71/root/usr/bin"
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer
