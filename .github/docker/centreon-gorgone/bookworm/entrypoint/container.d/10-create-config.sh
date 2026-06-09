#!/bin/sh
# Config is now static in the image (config.d/40-gorgoned.yaml).
# Dynamic params are injected at startup via env vars (GORGONE_UID, GORGONE_TOKEN,
# CENTRAL_HOST, CENTRAL_PORT) using gorgone's load_env_config mechanism.
