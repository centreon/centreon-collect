#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs dnf install < /tmp/build-dependencies.txt
#dnf install centreon-clib centreon-clib-devel

# Install PHPUnit.
curl -o /usr/local/bin/phpunit https://phar.phpunit.de/phpunit-8.phar
chmod +x /usr/local/bin/phpunit

# Install Xdebug PHP extension (for PHPUnit code coverage).
dnf install make php-devel
pecl install xdebug
echo 'zend_extension=/usr/lib64/php/modules/xdebug.so' >> /etc/php.d/10-xdebug.ini

# Install Node.js and related elements.
curl --silent --location https://rpm.nodesource.com/setup_12.x | bash -
dnf install --nogpgcheck -y nodejs
npm install -g gulp
npm install -g redoc-cli

# Install Composer.
dnf install -y php php-cli php-dom php-json php-mbstring
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Install Conan, a C++ package manager.
pip3 install conan
conan remote add centreon-center https://api.bintray.com/conan/centreon/centreon
conan remote remove conan-center
