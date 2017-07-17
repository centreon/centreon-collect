#!/bin/sh

set -e
set -x

# Base apt configuration.
echo 'APT::Get::Assume-Yes "true";' > /etc/apt/apt.conf.d/90assumeyes

# Install base tools.
apt-get update
apt-get install curl netcat-openbsd

# Install Node.js repository.
curl -sL https://deb.nodesource.com/setup_8.x | sudo -E bash -

# Install dependencies.
xargs apt-get install < /tmp/dependencies.txt
