#!/bin/sh

set -e
set -x

# Install development repository.
curl -o centreon-release.rpm "http://srvi-repo.int.centreon.com/yum/standard/21.04/el7/stable/noarch/RPMS/centreon-release-21.04-2.el7.centos.noarch.rpm"
yum install --nogpgcheck centreon-release.rpm
sed -i -e 's#yum.centreon.com#srvi-repo.int.centreon.com/yum#g' /etc/yum.repos.d/centreon.repo
yum-config-manager --enable 'centreon-testing*'
yum-config-manager --enable 'centreon-unstable*'

# Install Software Collections (for PHP 7).
curl -o centos-release-scl-rh.rpm "http://mirror.centos.org/centos-7/7/extras/x86_64/Packages/centos-release-scl-rh-2-3.el7.centos.noarch.rpm"
yum install centos-release-scl-rh.rpm
# Only keep centos-sclo-rh repository, as other repositories
# might be unavailable.
head -n 12 < /etc/yum.repos.d/CentOS-SCLo-scl-rh.repo > /tmp/scl-rh.repo
mv /tmp/scl-rh.repo /etc/yum.repos.d/CentOS-SCLo-scl-rh.repo

# Install required build dependencies for all Centreon projects.
xargs yum install < /tmp/build-dependencies.txt

# Install Node.js and related elements.
curl --silent --location https://rpm.nodesource.com/setup_14.x | bash -
# nodesource-release installs an invalid repository that we remove now.
head -n 8 /etc/yum.repos.d/nodesource-el7.repo > /etc/yum.repos.d/nodesource-el7.repo.new
mv /etc/yum.repos.d/nodesource-el7.repo{.new,}
yum install --nogpgcheck -y nodejs
npm install -g redoc-cli

# Install Composer.
yum install -y rh-php73-php rh-php73-php-cli rh-php73-php-dom rh-php73-php-mbstring rh-php73-php-intl rh-php73-php-pdo devtoolset-9
export PATH="$PATH:/opt/rh/rh-php73/root/usr/bin"
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Install PAR:Packer to build perl binaries
yum install -y perl perl-App-cpanminus perl-ExtUtils-Embed gcc openssl openssl-devel
cpanm PAR::Packer
cpanm PAR::Filter::Crypto

# Install Conan, a C++ package manager.
pip3 install --prefix=/usr conan

# Pre-install conan dependencies
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

conan install /tmp/conan-pkgs -s compiler.libcxx=libstdc++11 --build=missing
rm -rf /tmp/conan-pkgs

# Workaround, yum does not seem to exit correctly.
rm -f /var/run/yum.pid
