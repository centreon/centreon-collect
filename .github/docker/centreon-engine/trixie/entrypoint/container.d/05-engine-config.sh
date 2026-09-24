#!/bin/sh

# Patch centengine.cfg for Docker operation.
# rpc_listen_address is not yet exported by Centreon web — inject it here
# so the gRPC API is reachable from other containers (gorgone).
# Once the web export includes it, this becomes a no-op sed replace.
CFG="/etc/centreon-engine/centengine.cfg"

if [ -f "$CFG" ]; then
    if grep -q "^rpc_listen_address=" "$CFG"; then
        sed -i 's/^rpc_listen_address=.*/rpc_listen_address=0.0.0.0/' "$CFG"
    else
        echo "rpc_listen_address=0.0.0.0" >> "$CFG"
    fi
fi
