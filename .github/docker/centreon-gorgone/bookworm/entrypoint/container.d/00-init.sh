#!/bin/sh

rm -f /tmp/docker.ready

# Refresh apt package lists so plugins can be installed at runtime
# (lists are cleared during image build to reduce image size)
apt-get update -qq 2>&1 || true
