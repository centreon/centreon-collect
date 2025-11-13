#!/bin/sh

set -e

# Background apt-get for centreon-plugin* (kept as-is for now)
(
  export DEBIAN_FRONTEND=noninteractive
  sudo apt-get update -qq
  sudo apt-get install -y centreon-plugin* > /proc/1/fd/1 2>&1 || true
) &

# Use pre-installed Python venv from /opt/centreon/venv
# (fastapi and uvicorn are already installed in the Docker image)
export PATH="/opt/centreon/venv/bin:$PATH"
