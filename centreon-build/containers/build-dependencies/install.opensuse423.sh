#!/bin/sh

set -e
set -x

# Install required build dependencies for all Centreon projects.
xargs zypper install --downloadonly < /tmp/build-dependencies.txt
