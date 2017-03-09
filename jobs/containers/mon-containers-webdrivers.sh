#!/bin/sh

set -e
set -x

# Target WebDriver.
$browser=$1

# Pull base image.
docker pull ubuntu:rolling

# Build image.
cd centreon-build/containers
docker build -t ci.int.centreon.com:5000/mon-$browser:latest -f webdrivers/$browser.Dockerfile .

# Push image.
docker push ci.int.centreon.com:5000/mon-$browser:latest
