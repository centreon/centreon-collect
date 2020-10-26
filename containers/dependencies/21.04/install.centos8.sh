#!/bin/sh

set -e
set -x

# Base yum configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf
dnf install dnf-plugins-core
dnf config-manager --set-enabled 'PowerTools'

# Install base tools.
yum install curl nc

# Install Centreon repository.
curl -o centreon-release.rpm "http://srvi-repo.int.centreon.com/yum/standard/21.04/el8/stable/noarch/RPMS/centreon-release-21.04-2.el8.noarch.rpm"
dnf install --nogpgcheck centreon-release.rpm
sed -i -e 's#yum.centreon.com#srvi-repo.int.centreon.com/yum#g' /etc/yum.repos.d/centreon.repo
dnf config-manager --set-enabled 'centreon-testing*'

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_12.x | bash -

# Install dependencies.
xargs yum install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Clean packages
dnf config-manager --set-enabled 'centreon-unstable*'
dnf clean all
