#!/bin/sh

set -e
set -x

# Base apt configuration.
echo 'APT::Get::Assume-Yes "true";' > /etc/apt/apt.conf.d/90assumeyes

# Install base tools.
apt-get update
apt-get install curl netcat-openbsd

# Install Node.js repository.
curl --silent --location https://rpm.nodesource.com/setup_4.x | bash -

# Install dependencies.
xargs apt-get install < /tmp/dependencies.txt
