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
curl -o centreon-release.rpm "http://yum-1.centreon.com/standard/20.10/el8/stable/noarch/RPMS/centreon-release-20.10-2.el8.noarch.rpm"
curl -o centreon-bam-release.rpm "http://yum-1.centreon.com/centreon-bam/d4e1d7d3e888f596674453d1f20ff6d3/20.10/el8/stable/noarch/RPMS/centreon-bam-release-20.10-2.el8.noarch.rpm"
curl -o centreon-mbi-release.rpm "http://yum-1.centreon.com/centreon-mbi/5e0524c1c4773a938c44139ea9d8b4d7/20.10/el8/stable/noarch/RPMS/centreon-mbi-release-20.10-2.el8.noarch.rpm"
curl -o centreon-map-release.rpm "http://yum-1.centreon.com/centreon-map/bfcfef6922ae08bd2b641324188d8a5f/20.10/el8/stable/noarch/RPMS/centreon-map-release-20.10-2.el8.noarch.rpm"

dnf install --nogpgcheck centreon-release.rpm
dnf install --nogpgcheck centreon-bam-release.rpm
dnf install --nogpgcheck centreon-mbi-release.rpm
dnf install --nogpgcheck centreon-map-release.rpm 

dnf config-manager --set-enabled 'centreon-testing*'
dnf config-manager --set-enabled 'centreon-unstable*'

dnf config-manager --set-enabled 'centreon-bam-testing*'
dnf config-manager --set-enabled 'centreon-bam-unstable*'
dnf config-manager --set-enabled 'centreon-bam-testing-noarch'
dnf config-manager --set-enabled 'centreon-bam-unstable-noarch'

dnf config-manager --set-enabled 'centreon-map-testing*'
dnf config-manager --set-enabled 'centreon-map-unstable*'
dnf config-manager --set-enabled 'centreon-map-testing-noarch'
dnf config-manager --set-enabled 'centreon-map-unstable-noarch'

dnf config-manager --set-enabled 'centreon-mbi-testing*'
dnf config-manager --set-enabled 'centreon-mbi-unstable*'
dnf config-manager --set-enabled 'centreon-mbi-testing-noarch'
dnf config-manager --set-enabled 'centreon-mbi-unstable-noarch'

# Install Node.js.
curl --silent --location https://rpm.nodesource.com/setup_16.x | bash -

# Install dependencies.
xargs yum install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/php.d/centreon.ini

# Clean packages
dnf clean all
