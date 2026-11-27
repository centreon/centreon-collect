#!/bin/sh

# Patch centengine.cfg for Docker operation.
# rpc_listen_address is not yet exported by Centreon web — inject it here
# so the gRPC API is reachable from other containers (gorgone).
# Once the web export includes it, this becomes a no-op sed replace.
# ENGINE_RPC_LISTEN_ADDRESS overrides the address (e.g. 127.0.0.1 when no
# other container needs the Engine gRPC API, as in a gorgone-less stack).
CFG="/etc/centreon-engine/centengine.cfg"
ADDR="${ENGINE_RPC_LISTEN_ADDRESS:-0.0.0.0}"

if [ -f "$CFG" ]; then
    if grep -q "^rpc_listen_address=" "$CFG"; then
        sed -i "s/^rpc_listen_address=.*/rpc_listen_address=${ADDR}/" "$CFG"
    else
        echo "rpc_listen_address=${ADDR}" >> "$CFG"
    fi
fi
