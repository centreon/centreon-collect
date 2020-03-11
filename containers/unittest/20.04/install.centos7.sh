#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs yum install < /tmp/build-dependencies.txt

# Install PHPUnit.
curl -o /usr/local/bin/phpunit https://phar.phpunit.de/phpunit-8.phar
chmod +x /usr/local/bin/phpunit

# Install Xdebug PHP extension (for PHPUnit code coverage).
yum install sclo-php72-php-pecl-xdebug

# Install Composer.
yum install -y rh-php72-php rh-php72-php-cli rh-php72-php-dom rh-php72-php-mbstring
export PATH="$PATH:/opt/rh/rh-php72/root/usr/bin"
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Install Conan, a C++ package manager.
pip3 install conan
conan remote add centreon-center https://api.bintray.com/conan/centreon/centreon
conan remote remove conan-center
