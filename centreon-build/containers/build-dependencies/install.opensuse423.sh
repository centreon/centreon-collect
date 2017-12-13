#!/bin/sh

set -e
set -x

# Install required build dependencies for all Centreon projects.
xargs zypper --non-interactive install --download-only < /tmp/build-dependencies.txt
