#!/bin/sh

# Ensure centengine gRPC listens on all interfaces for inter-container communication.
# Centreon web export does not yet set rpc_listen_address, so we inject it here.
# When the web export eventually includes it, this script becomes a no-op (sed replace).
CFG="/etc/centreon-engine/centengine.cfg"

if [ -f "$CFG" ]; then
    if grep -q "^rpc_listen_address=" "$CFG"; then
        sed -i 's/^rpc_listen_address=.*/rpc_listen_address=0.0.0.0/' "$CFG"
    else
        echo "rpc_listen_address=0.0.0.0" >> "$CFG"
    fi
fi
