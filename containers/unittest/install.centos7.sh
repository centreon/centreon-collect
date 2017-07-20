#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs yum install < /tmp/build-dependencies.txt

# Install PHPUnit (the 4.8 release being the latest to support PHP 5.3).
curl -o /usr/local/bin/phpunit https://phar.phpunit.de/phpunit-4.8.23.phar
chmod +x /usr/local/bin/phpunit

# Install Xdebug PHP extension (for PHPUnit code coverage).
curl -o epel-release.rpm https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
yum install --nogpgcheck -y epel-release.rpm
yum install php-pecl-xdebug

# Install Composer.
yum install -y php php-cli php-dom php-mbstring
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer
