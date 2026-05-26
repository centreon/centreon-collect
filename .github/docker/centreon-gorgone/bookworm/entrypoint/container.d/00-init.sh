#!/bin/sh

rm -f /tmp/docker.ready

# Docker named volumes are always initialized root:root at runtime regardless of
# Dockerfile chown. Fix ownership here since gorgone mounts this volume first.
if [ "$(stat -c %u /var/lib/centreon-engine/rw 2>/dev/null)" != "901" ]; then
    sudo chown centreon-engine:centreon-engine /var/lib/centreon-engine/rw
fi

# Refresh apt package lists so plugins can be installed at runtime
# (lists are cleared during image build to reduce image size)
sudo apt-get update -qq 2>&1 || true
