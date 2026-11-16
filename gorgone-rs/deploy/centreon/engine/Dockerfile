# syntax=docker/dockerfile:1

# centreon-engine image with a centengine built from this branch, for the
# gorgone-rs proof of concept: it adds the ProcessExternalCommand and
# VerifyConfiguration RPCs, the gRPC API on a Unix domain socket
# (rpc_listen_address=unix:/path) and the recurringdowntime configuration
# object.
# The build context holds five files (see README.md), built with the packaging
# image of the same distribution: `centengine`, `libcentreon_engine_conf.so`,
# `externalcmd.so`, `libopentelemetry.so`, and `05-engine-config.sh`, the
# entrypoint script of this branch that reads ENGINE_RPC_LISTEN_ADDRESS.

ARG BASE_IMAGE=docker.centreon.com/centreon/centreon-engine-trixie:develop
FROM ${BASE_IMAGE}

# enginerpc is linked statically into centengine.
COPY --chown=root:root --chmod=755 centengine /usr/sbin/centengine

# state.proto has a new object (recurringdowntime): the configuration library
# and the two modules that use the configuration classes (externalcmd reads
# pb_config, libopentelemetry links the library) come from the same build.
# /lib is /usr/lib in the base image (merged /usr).
COPY --chown=root:root --chmod=755 libcentreon_engine_conf.so \
     /usr/lib/libcentreon_engine_conf.so
COPY --chown=root:root --chmod=755 externalcmd.so libopentelemetry.so \
     /usr/lib64/centreon-engine/

# Same place and mode as in the base image
# (.github/docker/centreon-engine/trixie/Dockerfile).
COPY --chown=root:root --chmod=755 05-engine-config.sh \
     /usr/local/lib/centreon-engine/container.d/05-engine-config.sh

# The binary still loads cbmod.so and libcentreon_clib.so from the base image
# packages: cbmod does not use the configuration classes.
RUN ! ldd /usr/sbin/centengine | grep "not found" \
    && ! ldd /usr/lib64/centreon-engine/libopentelemetry.so | grep "not found" \
    && /usr/sbin/centengine -V
