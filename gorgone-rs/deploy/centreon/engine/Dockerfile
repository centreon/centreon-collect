# syntax=docker/dockerfile:1

# centreon-engine image with a centengine built from this branch, for the
# gorgone-rs proof of concept: it adds the ProcessExternalCommand and
# VerifyConfiguration RPCs and the gRPC API on a Unix domain socket
# (rpc_listen_address=unix:/path).
# The build context holds two files (see README.md): `centengine`, built with
# the packaging image of the same distribution, and `05-engine-config.sh`,
# the entrypoint script of this branch that reads ENGINE_RPC_LISTEN_ADDRESS.

ARG BASE_IMAGE=docker.centreon.com/centreon/centreon-engine-trixie:develop
FROM ${BASE_IMAGE}

# enginerpc is linked statically into centengine: no other file changes.
COPY --chown=root:root --chmod=755 centengine /usr/sbin/centengine

# Same place and mode as in the base image
# (.github/docker/centreon-engine/trixie/Dockerfile).
COPY --chown=root:root --chmod=755 05-engine-config.sh \
     /usr/local/lib/centreon-engine/container.d/05-engine-config.sh

# The binary still loads the shared objects of the base image packages
# (cbmod.so, libcentreon_clib.so, libcentreon_engine_conf.so).
RUN ! ldd /usr/sbin/centengine | grep "not found" && /usr/sbin/centengine -V
