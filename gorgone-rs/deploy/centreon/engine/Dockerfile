# syntax=docker/dockerfile:1

# centreon-engine image with a centengine built from this branch, for the
# gorgone-rs proof of concept: it adds the ProcessExternalCommand and
# VerifyConfiguration RPCs, the gRPC API on a Unix domain socket
# (rpc_listen_address=unix:/path) and the recurringdowntime configuration
# object. It is driven through that API only: no external command file.
# The build context (see README.md), built with the packaging image of the
# same distribution: `centengine`, `libcentreon_engine_conf.so`,
# `libopentelemetry.so`, and `entrypoint/`, the entrypoint scripts of this
# branch (.github/docker/centreon-engine/trixie/entrypoint).

ARG BASE_IMAGE=docker.centreon.com/centreon/centreon-engine-trixie:develop
FROM ${BASE_IMAGE} AS rootfs

USER root

# enginerpc is linked statically into centengine.
COPY --chown=root:root --chmod=755 centengine /usr/sbin/centengine

# state.proto has a new object (recurringdowntime): the configuration library
# and libopentelemetry, which links it, come from the same build.
# /lib is /usr/lib in the base image (merged /usr).
COPY --chown=root:root --chmod=755 libcentreon_engine_conf.so \
     /usr/lib/libcentreon_engine_conf.so
COPY --chown=root:root --chmod=755 libopentelemetry.so /usr/lib64/centreon-engine/

# Same place and mode as in the base image
# (.github/docker/centreon-engine/trixie/Dockerfile). The plugin installer is
# Perl (check_packages.pl): the Python scripts of the base image go.
COPY --chown=root:root --chmod=755 entrypoint/ /usr/local/lib/centreon-engine/

# - No external command file: the externalcmd.so module (the FIFO reader) is
#   removed, and so are its broker_module line and command_file from the
#   packaged centengine.cfg, used until the first deployment. Engine itself
#   never creates the FIFO.
# - bsd-mailx gives the same `mail` command as mailutils, in front of the same
#   msmtp, without the Guile runtime mailutils pulls in (about 55 MB).
# - Python served only the plugin installer: purged (about 35 MB).
RUN rm -f /usr/lib64/centreon-engine/externalcmd.so \
       /usr/local/lib/centreon-engine/check_plugins.py \
       /usr/local/lib/centreon-engine/check_custom_deps.py \
    && sed -i -e '/^broker_module=.*externalcmd\.so/d' -e '/^command_file=/d' \
       /etc/centreon-engine/centengine.cfg \
    && apt-get update \
    && apt-get install -y --no-install-recommends bsd-mailx \
    && apt-get purge -y --auto-remove mailutils \
       python3-minimal python3.13-minimal libpython3.13-minimal libpython3.13-stdlib \
    && rm -rf /var/lib/apt/lists/* /var/cache/apt/* /var/log/apt/* /var/log/dpkg.log \
       /etc/python3.13 /usr/lib/python3.13 /usr/lib/python3 \
    && command -v mail \
    && ! command -v python3 \
    && ! grep -E '^(command_file=|broker_module=.*externalcmd)' /etc/centreon-engine/centengine.cfg \
    && perl -MJSON::PP -MHTTP::Tiny -MMIME::Base64 -e 1

# The binary still loads cbmod.so and libcentreon_clib.so from the base image
# packages: cbmod does not use the configuration classes.
RUN ! ldd /usr/sbin/centengine | grep "not found" \
    && ! ldd /usr/lib64/centreon-engine/libopentelemetry.so | grep "not found" \
    && /usr/sbin/centengine -V

# One layer: the base image and the files above replace or delete files of
# lower layers, which would otherwise still be shipped (about 200 MB).
FROM scratch

COPY --from=rootfs / /

# Same run time settings as the base image.
ENV PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    DEBIAN_FRONTEND=noninteractive \
    GORGONE_UID= \
    NAME= \
    GORGONE_TOKEN= \
    CENTRAL_HOST= \
    APP_SECRET= \
    SALT= \
    SMTP_HOST= \
    SMTP_PORT=25 \
    SMTP_FROM=centreon-engine@localhost \
    SMTP_TLS=off
EXPOSE 50155
WORKDIR /var/lib/centreon-engine
USER centreon-engine
HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=3 \
    CMD test -f /tmp/docker.ready || exit 1
ENTRYPOINT ["/usr/local/lib/centreon-engine/container.sh"]
CMD ["/usr/sbin/centengine", "/etc/centreon-engine/centengine.cfg"]
