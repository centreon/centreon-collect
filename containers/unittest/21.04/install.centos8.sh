#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs dnf install < /tmp/build-dependencies.txt
dnf install centreon-clib centreon-clib-devel

# Install PHPUnit.
curl -o /usr/local/bin/phpunit https://phar.phpunit.de/phpunit-8.phar
chmod +x /usr/local/bin/phpunit

# Install Xdebug PHP extension (for PHPUnit code coverage).
dnf install make php-devel
pecl install xdebug
echo 'zend_extension=/usr/lib64/php/modules/xdebug.so' >> /etc/php.d/10-xdebug.ini

# Install Node.js and related elements.
curl --silent --location https://rpm.nodesource.com/setup_16.x | bash -
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

mkdir /tmp/conan-pkgs
cat <<EOF >/tmp/conan-pkgs/conanfile.txt
[requires]
gtest/cci.20210126
asio/1.18.1
fmt/7.1.3
spdlog/1.8.5
nlohmann_json/3.9.1
openssl/1.1.1k
protobuf/3.15.5
grpc/1.37.0
mariadb-connector-c/3.1.12
zlib/1.2.11

[generators]
cmake_paths
cmake_find_package
EOF

conan install /tmp/conan-pkgs -s compiler.libcxx=libstdc++11 --build=missing
rm -rf /tmp/conan-pkgs
