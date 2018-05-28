#!/bin/sh

set -e
set -x

# Base yum configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf

# Install base tools.
yum install curl nc

# Install Centreon repository.
curl -o centreon-release.rpm "http://yum.centreon.com/standard/3.4/el7/stable/noarch/RPMS/centreon-release-3.4-4.el7.centos.noarch.rpm"
yum install --nogpgcheck centreon-release.rpm

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_8.x | bash -

# Install Software Collections repository.
curl -o centos-release-scl.rpm "http://mirror.centos.org/centos-7/7/extras/x86_64/Packages/centos-release-scl-2-2.el7.centos.noarch.rpm"
yum install --nogpgcheck centos-release-scl.rpm

# Install dependencies.
xargs yum install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Clean packages
yum clean all
