#!/bin/sh

set -e
set -x

# Base yum configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf

# Install base tools.
yum install curl nc

# Install Centreon repository.
curl -o centreon-release.rpm "http://yum.centreon.com/standard/3.4/el6/stable/noarch/RPMS/centreon-release-3.4-4.el6.noarch.rpm"
yum install --nogpgcheck centreon-release.rpm

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_4.x | bash -
yum install nodejs

# Install dependencies.
xargs yum install < /tmp/dependencies.txt
