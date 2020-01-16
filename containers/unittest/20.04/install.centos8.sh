#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs dnf install < /tmp/build-dependencies.txt

# Install PHPUnit.
curl -o /usr/local/bin/phpunit https://phar.phpunit.de/phpunit-8.phar
chmod +x /usr/local/bin/phpunit

# Install Xdebug PHP extension (for PHPUnit code coverage).
dnf install make php-devel php-pecl
pecl install debug
echo 'zend_extension=/usr/lib64/php/modules/xdebug.so' >> /etc/php.d/10-xdebug.ini

# Install Composer.
dnf install -y php php-cli php-dom php-json php-mbstring
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer
