#!/bin/sh

rm -f /tmp/docker.ready

# log_v2_logger=stdout is set by 05-engine-config.sh, so centengine logs go to
# Docker stdout directly — no log file used. Clear it anyway for clean restarts.
> /var/log/centreon-engine/centengine.log 2>/dev/null || true
rm -rf /var/log/centreon-engine/archives/* 2>/dev/null || true

# Refresh apt package lists so plugins can be installed at runtime
# (lists are cleared during image build to reduce image size)
sudo apt-get update -qq 2>&1 || true
