#!/bin/sh

# Patch centengine.cfg for Docker operation.
# rpc_listen_address is not yet exported by Centreon web — inject it here
# so the gRPC API is reachable from other containers (gorgone).
#
# ENGINE_RPC_LISTEN_ADDRESS (optional, default 0.0.0.0) is the value written:
# an address to listen on with grpc_port, or unix:/absolute/path to serve the
# API on a Unix domain socket only (mode 0660, no TCP listener), for a sidecar
# sharing the volume that holds the socket.
CFG="/etc/centreon-engine/centengine.cfg"
RPC_LISTEN_ADDRESS="${ENGINE_RPC_LISTEN_ADDRESS:-0.0.0.0}"

if [ -f "$CFG" ]; then
    # Delete then append: the value may hold any character (a path has "/").
    sed -i '/^rpc_listen_address=/d' "$CFG"
    echo "rpc_listen_address=$RPC_LISTEN_ADDRESS" >> "$CFG"
fi
