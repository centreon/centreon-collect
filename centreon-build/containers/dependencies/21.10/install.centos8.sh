#!/bin/sh

set -e

# Clean packages
dnf clean all

# Base yum configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf
dnf install dnf-plugins-core
dnf config-manager --set-enabled 'PowerTools'

# Install base tools.
yum install curl nc

# Install Centreon repository.
curl -o centreon-release.rpm "http://yum-1.centreon.com/standard/21.10/el8/stable/noarch/RPMS/centreon-release-21.10-1.el8.noarch.rpm"
dnf install --nogpgcheck centreon-release.rpm
dnf config-manager --set-enabled 'centreon-testing*'

# Switch AppStream to install php73
dnf module enable php:7.3

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_16.x | bash -

# Install dependencies.
xargs yum install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Clean packages
dnf config-manager --set-enabled 'centreon-unstable*'
dnf clean all
