# syntax=docker/dockerfile:1

# centreon-engine image with a centengine built from this branch, for the
# gorgone-rs proof of concept: it adds the ProcessExternalCommand RPC.
# The build context holds one file, `centengine`, built with the packaging
# image of the same distribution (see README.md).

ARG BASE_IMAGE=docker.centreon.com/centreon/centreon-engine-trixie:develop
FROM ${BASE_IMAGE}

# enginerpc is linked statically into centengine: no other file changes.
COPY --chown=root:root --chmod=755 centengine /usr/sbin/centengine

# The binary still loads the shared objects of the base image packages
# (cbmod.so, libcentreon_clib.so, libcentreon_engine_conf.so).
RUN ! ldd /usr/sbin/centengine | grep "not found" && /usr/sbin/centengine -V
