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

# Install Centreon repositories.
curl -o centreon-release.rpm "http://yum-1.centreon.com/standard/20.10/el7/stable/noarch/RPMS/centreon-release-20.10-2.el7.centos.noarch.rpm"
curl -o centreon-bam-release.rpm "http://yum-1.centreon.com/centreon-bam/d4e1d7d3e888f596674453d1f20ff6d3/20.10/el7/stable/noarch/RPMS/centreon-bam-release-20.10-2.el7.centos.noarch.rpm"
curl -o centreon-mbi-release.rpm "http://yum-1.centreon.com/centreon-mbi/5e0524c1c4773a938c44139ea9d8b4d7/20.10/el7/stable/noarch/RPMS/centreon-mbi-release-20.10-2.el7.centos.noarch.rpm"
curl -o centreon-map-release.rpm "http://yum-1.centreon.com/centreon-map/bfcfef6922ae08bd2b641324188d8a5f/20.10/el7/stable/noarch/RPMS/centreon-map-release-20.10-2.el7.centos.noarch.rpm"

yum install --nogpgcheck centreon-release.rpm
yum install --nogpgcheck centreon-bam-release.rpm
yum install --nogpgcheck centreon-mbi-release.rpm
yum install --nogpgcheck centreon-map-release.rpm 

yum-config-manager --enable 'centreon-testing*'
yum-config-manager --enable 'centreon-unstable*'

yum-config-manager --enable 'centreon-bam-testing*'
yum-config-manager --enable 'centreon-bam-unstable*'
yum-config-manager --enable 'centreon-bam-testing-noarch'
yum-config-manager --enable 'centreon-bam-unstable-noarch'

yum-config-manager --enable 'centreon-map-testing*'
yum-config-manager --enable 'centreon-map-unstable*'
yum-config-manager --enable 'centreon-map-testing-noarch'
yum-config-manager --enable 'centreon-map-unstable-noarch'

yum-config-manager --enable 'centreon-mbi-testing*'
yum-config-manager --enable 'centreon-mbi-unstable*'
yum-config-manager --enable 'centreon-mbi-testing-noarch'
yum-config-manager --enable 'centreon-mbi-unstable-noarch'

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_16.x | bash -

# Install Software Collections repository.
yum install centos-release-scl

# Install dependencies.
xargs yum install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/opt/rh/rh-php72/php.d/centreon.ini

# Clean packages
yum clean all
