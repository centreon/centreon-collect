#!/bin/sh

set -e
set -x

# Install development repository.
curl -o centreon-release.rpm "http://srvi-repo.int.centreon.com/yum/standard/19.4/el7/stable/noarch/RPMS/centreon-release-19.4-1.el7.centos.noarch.rpm"
yum install --nogpgcheck centreon-release.rpm
sed -i -e 's#yum.centreon.com#srvi-repo.int.centreon.com/yum#g' /etc/yum.repos.d/centreon.repo
yum-config-manager --enable 'centreon-testing*'

# Install Software Collections (for PHP 7).
curl -o centos-release-scl-rh.rpm "http://mirror.centos.org/centos-7/7/extras/x86_64/Packages/centos-release-scl-rh-2-2.el7.centos.noarch.rpm"
yum install centos-release-scl-rh.rpm

# Install required build dependencies for all Centreon projects.
xargs yum install < /tmp/build-dependencies.txt

# Install Node.js and related elements.
curl --silent --location https://rpm.nodesource.com/setup_10.x | bash -
yum install --nogpgcheck -y nodejs
npm install -g gulp

# Install Composer.
yum install -y rh-php71-php rh-php71-php-cli rh-php71-php-dom rh-php71-php-mbstring
export PATH="$PATH:/opt/rh/rh-php71/root/usr/bin"
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Install PAR::Packer to build perl binaries
yum install -y perl-core perl-devel openssl openssl-devel
curl --silent --location http://xrl.us/cpanm | perl - PAR::Packer PAR::Filter::Crypto --force

# Workaround, yum does not seem to exit correctly.
rm -f /var/run/yum.pid
