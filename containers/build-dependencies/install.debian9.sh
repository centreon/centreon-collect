#!/bin/sh

set -e
set -x

# Base apt configuration.
echo 'APT::Get::Assume-Yes "true";' > /etc/apt/apt.conf.d/90assumeyes

# Install base tools.
apt-get update
apt-get install curl gnupg

# Install Node.js repository.
curl -sL https://deb.nodesource.com/setup_8.x | bash -

# Install required build dependencies for all Centreon projects.
apt-get update
xargs apt-get install -d < /tmp/build-dependencies.txt

# Install Node.js.
apt-get install nodejs
npm install -g gulp

# Install Composer.
apt-get install php php-cli
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer
