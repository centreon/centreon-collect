#!/bin/sh

set -e
set -x

# Base yum configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf

# Install base tools.
yum install curl nc

# Install Centreon repository.
curl -o centreon-release.rpm "http://srvi-repo.int.centreon.com/yum/lts/3.4/el7/stable/noarch/RPMS/centreon-release-3.4-5.el7.centos.noarch.rpm"
yum install --nogpgcheck centreon-release.rpm

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_6.x | bash -

# Install dependencies.
xargs yum install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Clean packages
yum clean all
