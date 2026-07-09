#!/bin/sh

rm -f /tmp/docker.ready

# centengine logs to this file (log_v2_logger=file); 10-log-tail_background.sh
# streams it to stdout. Clear it so tail -F starts clean on restart.
> /var/log/centreon-engine/centengine.log 2>/dev/null || true
rm -rf /var/log/centreon-engine/archives/* 2>/dev/null || true

# Refresh apt package lists so plugins can be installed at runtime
# (lists are cleared during image build to reduce image size)
sudo apt-get update -qq 2>&1 || true
