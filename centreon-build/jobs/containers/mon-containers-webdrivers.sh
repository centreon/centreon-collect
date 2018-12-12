#!/bin/sh

set -e
set -x

# Target WebDriver.
browser=$1

# Pull base image.
docker pull ubuntu:rolling

# Build image.
cd `dirname $0`/../../containers
docker build -t registry.centreon.com/mon-$browser:latest -f webdrivers/$browser.Dockerfile .

# Push image.
docker push registry.centreon.com/mon-$browser:latest
