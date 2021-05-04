#!/bin/sh

set -e
set -x

# Install build dependencies.
xargs yum install < /tmp/build-dependencies.txt
yum install centreon-clib centreon-clib-devel

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
yum install -y epel-release cmake3 devtoolset-9 perl-Thread-Queue
yum remove epel-release cmake

# Install Conan, a C++ package manager.
pip3 install conan

# Pre-install dependencies
ln -s /usr/bin/cmake3 /usr/bin/cmake
source /opt/rh/devtoolset-9/enable

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

conan install /tmp/conan-pkgs -s compiler.libcxx=libstdc++11 --build='*'
rm -rf /tmp/conan-pkgs
