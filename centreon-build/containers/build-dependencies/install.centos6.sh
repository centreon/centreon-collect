#!/bin/sh

set -e
set -x

# import gpg key
gpg --import /tmp/ces.key

# Install development repository.
curl -o centreon-release.rpm http://yum.centreon.com/standard/3.4/el6/stable/noarch/RPMS/centreon-release-3.4-4.el6.noarch.rpm
yum install --nogpgcheck centreon-release.rpm
sed -e 's/@VERSION@/3.4/g' -e 's/@DISTRIB@/el6/g' < /tmp/centreon-internal.repo.in > /etc/yum.repos.d/centreon-internal.repo

# Install required build dependencies for all Centreon projects.
xargs yum install --downloadonly < /tmp/build-dependencies.txt

# Install Node.js and related elements.
curl --silent --location https://rpm.nodesource.com/setup_6.x | bash -
yum install --nogpgcheck -y nodejs
npm install -g gulp

# Install Composer.
yum install -y php php-cli php-dom php-mbstring
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer

# Install PAR::Packer to build perl binaries
yum install -y perl-core perl-devel openssl openssl-devel
curl --silent --location http://xrl.us/cpanm | perl - PAR::Packer PAR::Filter::Crypto --force

# Workaround, yum does not seem to exit correctly.
rm -f /var/run/yum.pid
