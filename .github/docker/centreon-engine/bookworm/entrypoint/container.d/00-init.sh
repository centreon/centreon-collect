#!/bin/sh

rm -f /tmp/docker.ready

# Clear log file and archives on each start — retention.dat and status.dat are kept by the volume
> /var/log/centreon-engine/centengine.log 2>/dev/null || true
rm -rf /var/log/centreon-engine/archives/* 2>/dev/null || true

# Refresh apt package lists so plugins can be installed at runtime
# (lists are cleared during image build to reduce image size)
sudo apt-get update -qq 2>&1 || true
