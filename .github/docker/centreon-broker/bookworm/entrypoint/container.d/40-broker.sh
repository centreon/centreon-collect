#!/bin/sh

set -e

# Use pre-installed Python venv from /opt/centreon/venv
# (fastapi and uvicorn are already installed in the Docker image)
export PATH="/opt/centreon/venv/bin:$PATH"
