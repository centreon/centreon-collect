#!/bin/sh

set -e
set -x

# Base yum configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf
dnf install dnf-plugins-core
dnf install langpacks-en glibc-all-langpacks -y
dnf config-manager --set-enabled 'PowerTools'

# Install base tools.
yum install curl nc

# Install Centreon repositories.
curl -o centreon-release.rpm "http://yum-1.centreon.com/standard/21.04/el7/stable/noarch/RPMS/centreon-release-21.04-4.el7.centos.noarch.rpm"
yum install --nogpgcheck centreon-release.rpm
curl -o centreon-release-business.rpm "http://yum-1.centreon.com/centreon-business/1a97ff9985262bf3daf7a0919f9c59a6/21.04/el8/stable/noarch/RPMS/centreon-business-release-21.04-4.el8.noarch.rpm"
yum install --nogpgcheck centreon-release-business.rpm
yum-config-manager --enable 'centreon-testing*'
yum-config-manager --enable 'centreon-unstable*'
yum-config-manager --enable 'centreon-business-testing*'
yum-config-manager --enable 'centreon-business-unstable*'
yum-config-manager --enable 'centreon-business-testing-noarch'
yum-config-manager --enable 'centreon-business-unstable-noarch'

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
