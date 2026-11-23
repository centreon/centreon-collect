#!/bin/sh

rm -f /tmp/docker.ready

# The command FIFO directory may be a tmpfs (no state to keep). The container
# runs without root, so the mount must already belong to centreon-engine
# (e.g. tmpfs option uid=901,gid=901,mode=0770).
RW_DIR=/var/lib/centreon-engine/rw
mkdir -p "$RW_DIR" 2>/dev/null || true
if [ ! -w "$RW_DIR" ]; then
    echo "init: ERROR: $RW_DIR is not writable by $(id -un)" \
         "(mount it as tmpfs with uid=901,gid=901,mode=0770)" >&2
    exit 1
fi

# centengine logs to this file (log_v2_logger=file); 10-log-tail_background.sh
# streams it to stdout. Clear it so tail -F starts clean on restart.
> /var/log/centreon-engine/centengine.log 2>/dev/null || true
rm -rf /var/log/centreon-engine/archives/* 2>/dev/null || true

# Refresh apt package lists so plugins can be installed at runtime
# (lists are cleared during image build to reduce image size)
sudo apt-get update -qq 2>&1 || true
