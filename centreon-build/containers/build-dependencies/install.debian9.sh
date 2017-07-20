#!/bin/sh

set -e
set -x

# Base apt configuration.
echo 'APT::Get::Assume-Yes "true";' > /etc/apt/apt.conf.d/90assumeyes

# Install base tools.
apt-get update
apt-get install curl gnupg

# Install development repository.
curl http://srvi-repo.int.centreon.com/apt/centreon.apt.gpg | apt-key add -
echo 'deb http://srvi-repo.int.centreon.com/apt/internal/3.5 stretch main' > /etc/apt/sources.list.d/centreon-internal.list

# Install Node.js repository.
curl -sL https://deb.nodesource.com/setup_8.x | bash -

# Install required build dependencies for all Centreon projects.
apt-get update
xargs apt-get install < /tmp/build-dependencies.txt

# Install Node.js.
apt-get install nodejs
npm install -g gulp

# Install Composer.
apt-get install php php-cli php-curl php-mbstring php-xml
curl -sS https://getcomposer.org/installer | php
# For mon-build-dependencies, composer need to be in /usr/bin so that
# composer is still available during debuild.
mv composer.phar /usr/bin/composer
chmod +x /usr/bin/composer
