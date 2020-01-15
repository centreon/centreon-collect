#!/bin/sh

set -e
set -x

# Base yum configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf

# Install base tools.
yum install curl nc

# Install Centreon repository.
curl -o centreon-release.rpm "http://srvi-repo.int.centreon.com/yum/standard/20.04/el8/stable/noarch/RPMS/centreon-release-20.04-1.el8.noarch.rpm"
yum install --nogpgcheck centreon-release.rpm
sed -i -e 's#yum.centreon.com#srvi-repo.int.centreon.com/yum#g' /etc/yum.repos.d/centreon.repo
yum-config-manager --enable 'centreon-testing*'

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_12.x | bash -

# Install Software Collections repository.
yum install centos-release-scl

# Install dependencies.
xargs yum install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Clean packages
yum-config-manager --enable 'centreon-unstable*'
yum clean all
