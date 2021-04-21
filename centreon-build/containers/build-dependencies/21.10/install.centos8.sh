#!/bin/sh

set -e
set -x

# Base dnf configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf
sed -i 's/best=True/best=False/g' /etc/dnf/dnf.conf
dnf install dnf-plugins-core
dnf config-manager --set-enabled 'PowerTools'

# Install development repository.
curl -o centreon-release.rpm "http://srvi-repo.int.centreon.com/yum/standard/21.10/el8/stable/noarch/RPMS/centreon-release-21.10-1.el8.noarch.rpm"
dnf install --nogpgcheck centreon-release.rpm
sed -i -e 's#yum.centreon.com#srvi-repo.int.centreon.com/yum#g' /etc/yum.repos.d/centreon.repo
dnf config-manager --set-enabled 'centreon-testing*'

# Switch AppStream to install php73
dnf module enable php:7.3 -y

# Install required build dependencies for all Centreon projects.
xargs dnf install < /tmp/build-dependencies.txt

# Install Node.js and related elements.
curl --silent --location https://rpm.nodesource.com/setup_14.x | bash -
dnf install --nogpgcheck -y nodejs
npm install -g redoc-cli

# Install Composer.
dnf install php php-cli php-dom php-json php-mbstring php-intl
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Install Conan, a C++ package manager.
pip3 install --prefix=/usr conan
conan remote add centreon-center https://api.bintray.com/conan/centreon/centreon
conan remote remove conan-center

# Enable unstable repositories.
dnf config-manager --set-enabled 'centreon-unstable*'
