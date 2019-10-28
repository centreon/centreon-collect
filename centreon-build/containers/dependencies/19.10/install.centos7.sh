#!/bin/sh

set -e
set -x

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

# Install Centreon repository.
curl -o centreon-release.rpm "http://srvi-repo.int.centreon.com/yum/standard/19.10/el7/stable/noarch/RPMS/centreon-release-19.10-1.el7.centos.noarch.rpm"
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
echo 'date.timezone = Europe/Paris' > /etc/opt/rh/rh-php72/php.d/centreon.ini

# Clean packages
yum-config-manager --enable 'centreon-unstable*'
yum clean all
