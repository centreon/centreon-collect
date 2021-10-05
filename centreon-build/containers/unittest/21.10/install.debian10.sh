#!/bin/sh

set -e
set -x

# Update repository information.
apt-get update

# Install build dependencies.
xargs apt-get install < /tmp/build-dependencies.txt

# Install PHPunit and Xdebug (for PHPunit code coverage).
apt-get install phpunit php-xdebug

# Install Composer.
apt-get install php php-cli php-curl php-mbstring php-xml
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Install Conan, a C++ package manager.
# Pip is installed through the official repository because of urllib3 that is
# too old when provided by Debian 10 repo.
curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
python3 get-pip.py
pip3 install conan
ln -s /usr/local/bin/conan /usr/bin/conan

# Pre-install dependencies
mkdir /tmp/conan-pkgs
cat <<EOF >/tmp/conan-pkgs/conanfile.txt
[requires]
gtest/cci.20210126
asio/1.18.1
fmt/7.1.3
spdlog/1.8.5
nlohmann_json/3.9.1
openssl/1.1.1l
grpc/1.37.0
mariadb-connector-c/3.1.12
zlib/1.2.11

[generators]
cmake_paths
cmake_find_package
EOF

conan install /tmp/conan-pkgs -s compiler.libcxx=libstdc++11 --build=missing
rm -rf /tmp/conan-pkgs
