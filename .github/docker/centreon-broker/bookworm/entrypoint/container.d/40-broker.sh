#!/bin/sh

set -e

# create / reuse venv directory
if [ ! -d ./venv ]; then
  python3 -m venv ./venv
fi
# activate venv
. ./venv/bin/activate

pip install --no-cache-dir fastapi uvicorn
