#!/bin/sh

set -e
set -x

# Clean packages
yum clean all

# Base yum configuration.
echo 'http_caching=none' >> /etc/yum.conf
echo 'assumeyes=1' >> /etc/yum.conf
sed -i -e 's/\(override_install_langs=.*\)/\1:es_ES.utf8:fr_FR.utf8:pt_BR.utf8:pt_PT.utf8/' /etc/yum.conf
yum update glibc-common
yum reinstall glibc-common
localedef -i es_ES -f UTF-8 es_ES.UTF-8
localedef -i fr_FR -f UTF-8 fr_FR.UTF-8
localedef -i pt_BR -f UTF-8 pt_BR.UTF-8
localedef -i pt_PT -f UTF-8 pt_PT.UTF-8

# Install base tools.
yum install curl nc

# Install Centreon repositories.
curl -o centreon-release.rpm "http://yum-1.centreon.com/standard/21.10/el7/stable/noarch/RPMS/centreon-release-21.10-1.el7.centos.noarch.rpm"
yum install --nogpgcheck centreon-release.rpm
curl -o centreon-release-business.rpm http://yum-1.centreon.com/centreon-business/1a97ff9985262bf3daf7a0919f9c59a6/21.10/el7/stable/noarch/RPMS/centreon-business-release-21.10-1.el7.centos.noarch.rpm
yum install --nogpgcheck centreon-release-business.rpm
yum-config-manager --enable 'centreon-testing*'
yum-config-manager --enable 'centreon-unstable*'
yum-config-manager --enable 'centreon-business-testing*'
yum-config-manager --enable 'centreon-business-unstable*'
yum-config-manager --enable 'centreon-business-testing-noarch'
yum-config-manager --enable 'centreon-business-unstable-noarch'

# Install remi repository
curl -o remi-release-7.rpm https://rpms.remirepo.net/enterprise/remi-release-7.rpm
yum install remi-release-7.rpm
yum-config-manager --enable remi-php80

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_14.x | bash -
sudo npm cache clean -f
sudo npm install -g n
sudo n latest

# Install Software Collections repository.
yum install centos-release-scl

# Install dependencies.
xargs yum install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Clean packages
yum clean all
