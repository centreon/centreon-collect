#!/bin/sh

rm -f /tmp/docker.ready

# Replace log file with a named pipe so centengine logs go directly to Docker stdout.
# No file accumulation — Docker logging driver handles retention.
LOG="/var/log/centreon-engine/centengine.log"
rm -f "$LOG" 2>/dev/null || true
mkfifo "$LOG"
rm -rf /var/log/centreon-engine/archives/* 2>/dev/null || true

# Refresh apt package lists so plugins can be installed at runtime
# (lists are cleared during image build to reduce image size)
sudo apt-get update -qq 2>&1 || true
