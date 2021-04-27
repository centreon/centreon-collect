#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs yum install < /tmp/build-dependencies.txt
#yum install centreon-clib centreon-clib-devel

# Install PHPUnit.
curl -o /usr/local/bin/phpunit https://phar.phpunit.de/phpunit-8.phar
chmod +x /usr/local/bin/phpunit

# Install Xdebug PHP extension (for PHPUnit code coverage).
yum install rh-php73-php-pecl-xdebug

# Install Composer.
yum install -y rh-php73-php rh-php73-php-cli rh-php73-php-dom rh-php73-php-mbstring
export PATH="$PATH:/opt/rh/rh-php73/root/usr/bin"
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Install cmake3
yum install -y epel-release
yum install -y cmake3
yum remove epel-release

# Install Conan, a C++ package manager.
pip3 install conan
conan remote add centreon-center https://api.bintray.com/conan/centreon/centreon
conan remote remove conan-center
