#!/bin/sh

set -e
set -x

# Base dnf configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf
dnf install dnf-plugins-core
dnf config-manager --set-enabled 'PowerTools'

# Install development repository.
curl -o centreon-release.rpm "http://srvi-repo.int.centreon.com/yum/standard/20.04/el8/stable/noarch/RPMS/centreon-release-20.04-1.el8.noarch.rpm"
dnf install --nogpgcheck centreon-release.rpm
sed -i -e 's#yum.centreon.com#srvi-repo.int.centreon.com/yum#g' /etc/yum.repos.d/centreon.repo
dnf config-manager --set-enabled 'centreon-testing*'

# Install required build dependencies for all Centreon projects.
xargs dnf install < /tmp/build-dependencies.txt

# Install Node.js and related elements.
curl --silent --location https://rpm.nodesource.com/setup_12.x | bash -
dnf install --nogpgcheck -y nodejs
npm install -g gulp

# Install Composer.
dnf install php php-cli php-dom php-json php-mbstring
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Enable unstable repositories.
dnf config-manager --set-enabled 'centreon-unstable*'
