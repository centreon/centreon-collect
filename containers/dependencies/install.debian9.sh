#!/bin/sh

set -e
set -x

# Base apt configuration.
echo 'APT::Get::Assume-Yes "true";' > /etc/apt/apt.conf.d/90assumeyes

# Install base tools.
apt-get update
apt-get install curl gnupg netcat-openbsd

# Install Node.js repository.
curl -sL https://deb.nodesource.com/setup_8.x | bash -

# Install dependencies.
xargs apt-get install < /tmp/dependencies.txt

# Configuration.
echo 'date.timezone = Europe/Paris' > /etc/php/7.0/apache2/conf.d/50-centreon.ini
echo 'date.timezone = Europe/Paris' > /etc/php/7.0/cli/conf.d/50-centreon.ini
