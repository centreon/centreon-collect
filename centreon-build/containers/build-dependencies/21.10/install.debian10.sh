#!/bin/sh

set -e
set -x

# Base apt configuration.
echo 'APT::Get::Assume-Yes "true";' > /etc/apt/apt.conf.d/90assumeyes

# Install base tools.
apt-get update
apt-get install curl gnupg ca-certificates

# Trust centreon internal certificate
mkdir /usr/local/share/ca-certificates/int.centreon.com
cp /tmp/ca-centreon-internal.pem /usr/local/share/ca-certificates/int.centreon.com/ca-centreon-internal.crt
update-ca-certificates

# Install development repository.
curl http://srvi-repo.int.centreon.com/apt/centreon.apt.gpg | apt-key add -
echo 'deb http://srvi-repo.int.centreon.com/apt/internal/21.10 buster main' > /etc/apt/sources.list.d/centreon-internal.list

# Install Node.js repository.
curl -sL https://deb.nodesource.com/setup_14.x | bash -

# Install required build dependencies for all Centreon projects.
apt-get update
xargs apt-get install < /tmp/build-dependencies.txt

# Install Node.js.
apt-get install nodejs

# Install Composer.
apt-get install php php-cli php-curl php-mbstring php-xml php-pdo
curl -sS https://getcomposer.org/installer | php
# For mon-build-dependencies, composer need to be in /usr/bin so that
# composer is still available during debuild.
mv composer.phar /usr/bin/composer
chmod +x /usr/bin/composer

# Install Conan, a C++ package manager.
pip3 install conan
ln -s /usr/local/bin/conan /usr/bin/conan

# Pre-install conan dependencies
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
