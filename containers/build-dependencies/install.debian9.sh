#!/bin/sh

set -e
set -x

# Install Node.js repository.
curl -sL https://deb.nodesource.com/setup_8.x | bash -

# Install required build dependencies for all Centreon projects.
apt-get update
xargs apt-get install -d < /tmp/build-dependencies.txt

# Install Node.js.
apt-get install nodejs
npm install -g gulp

# Install Composer.
curl -sS https://getcomposer.org/installer | php
mv composer.phar /usr/local/bin/composer
chmod +x /usr/local/bin/composer
