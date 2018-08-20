#!/bin/sh

set -e
set -x

# Base yum configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf

# Install base tools.
yum install curl nc

# Install Centreon repository.
curl -o centreon-release.rpm "http://srvi-repo.int.centreon.com/yum/standard/18.9/el7/stable/noarch/RPMS/centreon-release-18.9-1.el7.centos.noarch.rpm"
yum install --nogpgcheck centreon-release.rpm
sed -i -e 's#yum.centreon.com#srvi-repo.int.centreon.com/yum#g' -e 's/enabled=0/enabled=1/g' /etc/yum.repos.d/centreon.repo

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_8.x | bash -

# Install Software Collections repository.
yum install centos-release-scl

# Install dependencies.
xargs yum install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/opt/rh/rh-php71/php.d/centreon.ini

# Clean packages
yum clean all
