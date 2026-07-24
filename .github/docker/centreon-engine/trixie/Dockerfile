# syntax=docker/dockerfile:1

#############################################
# Stage 1: Extract files from .deb packages
#############################################
FROM debian:13-slim AS extractor

ARG TARGETARCH

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
    dpkg file

# Extract .deb packages (mounted via --mount during build)
# Smart extraction: skip broker-cbd, extract all others, then clean up junk files
RUN --mount=type=bind,source=packages-centreon,target=/packages \
    mkdir -p /extracted && \
    cd /packages/${TARGETARCH} && \
    for deb in centreon*.deb; do \
        case "$deb" in \
            *broker-cbd*) continue ;; \
        esac; \
        dpkg-deb -x "$deb" /extracted/; \
    done && \
    rm -rf /extracted/usr/share/doc/* \
           /extracted/usr/share/man/* \
           /extracted/usr/share/info/* \
           /extracted/usr/share/locale/* \
           /extracted/usr/share/licenses/* \
           /extracted/var/cache/* \
           /extracted/var/log/*

#############################################
# Stage 2: Runtime - Minimal final image
#############################################
FROM debian:13-slim

ARG VERSION
ARG IS_CLOUD
ARG STABILITY

ENV DEBIAN_FRONTEND=noninteractive \
    GORGONE_UID= \
    NAME= \
    GORGONE_TOKEN= \
    CENTRAL_HOST= \
    APP_SECRET= \
    SALT=

# Create users and groups (must match current setup for compatibility)
RUN bash -e <<EOF
groupadd -g 33 www-data 2>/dev/null || true
groupadd -g 900 centreon
useradd -u 900 -g centreon -m -r -d /var/spool/centreon -c "Centreon" -s /bin/bash centreon
groupadd -g 901 centreon-engine
useradd -u 901 -g centreon-engine -m -r -d /var/lib/centreon-engine -c "Centreon Engine" -s /bin/bash centreon-engine
groupadd -g 902 centreon-broker
useradd -u 902 -g centreon-broker -m -r -d /var/lib/centreon-broker -c "Centreon Broker" -s /bin/bash centreon-broker
groupadd -g 903 centreon-gorgone
useradd -u 903 -g centreon-gorgone -m -r -d /var/lib/centreon-gorgone -c "Centreon Gorgone" -s /bin/bash centreon-gorgone
usermod -aG centreon-broker centreon-engine
usermod -aG centreon-engine centreon-broker
usermod -aG centreon-gorgone centreon-engine
usermod -aG centreon centreon-engine
usermod -aG www-data centreon-engine
usermod -aG sudo centreon-engine
EOF

# Configure APT for optimal container usage
COPY <<EOF /etc/apt/apt.conf.d/99custom
Acquire::Retries "10";
Acquire::https::Timeout "300";
Acquire::http::Timeout "300";
APT::Install-Recommends "false";
APT::Install-Suggests "false";
EOF

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    bash -e <<EOF
apt-get update
apt-get install -y --no-install-recommends gnupg wget ca-certificates
wget -qO- https://apt-key.centreon.com | gpg --dearmor > /etc/apt/trusted.gpg.d/centreon.gpg
echo "deb [arch=amd64,arm64] https://packages.centreon.com/apt-plugins-stable/ trixie main" > /etc/apt/sources.list.d/centreon-plugins.list
EOF

# apt-plugins-stable now publishes arm64 .deb packages for libnet-curl-perl,
# libcrypt-openssl-aes-perl and libssh-session-perl, so the CPAN compilation
# fallback (and the build-essential/dev headers it required) is no longer needed.
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    bash -e <<'EOF'
apt-get update
apt-get install -y --no-install-recommends \
    libnet-curl-perl \
    libcrypt-openssl-aes-perl \
    libssh-session-perl \
    python3-minimal \
    libmariadb3 \
    libgnutls30t64 \
    libssl3 \
    zlib1g \
    monitoring-plugins-basic \
    libzmq-constants-perl \
    libcap2-bin \
    inotify-tools \
    sudo \
    ca-certificates \
    wget \
    gnupg \
    centreon-plugin-applications-monitoring-centreon-poller
setcap cap_net_raw+ep /usr/lib/nagios/plugins/check_icmp
apt-get remove -y --purge libcap2-bin gnupg wget
apt-get autoremove -y
rm -rf /tmp/* /var/tmp/* /usr/share/doc/* /usr/share/man/* /usr/share/info/* /usr/share/locale/* /var/cache/apt/* /var/log/*
EOF

# Create directory structure
RUN bash -e <<EOF
mkdir -p /etc/centreon-engine \
         /etc/centreon-engine/conf.d \
         /etc/centreon-broker \
         /var/lib/centreon-engine \
         /var/lib/centreon-engine/rw \
         /var/log/centreon-engine \
         /var/log/centreon-engine/archives \
         /var/lib/centreon/centplugins \
         /usr/share/centreon/lib/centreon-broker \
         /usr/lib64/centreon-engine \
         /usr/lib/nagios/plugins \
         /usr/lib/nagios/plugins/custom \
         /usr/lib64/nagios
ln -s /usr/lib/nagios/plugins /usr/lib64/nagios/plugins
chown -R centreon-engine:centreon-engine \
         /etc/centreon-engine \
         /var/lib/centreon-engine \
         /var/log/centreon-engine \
         /var/lib/centreon/centplugins \
         /usr/lib/nagios/plugins/custom
chown -R centreon-broker:centreon-broker \
          /etc/centreon-broker
chmod 775 /etc/centreon-engine \
          /etc/centreon-broker \
          /etc/centreon-engine/conf.d \
          /var/lib/centreon-engine \
          /var/lib/centreon-engine/rw
chmod 755 /var/log/centreon-engine \
          /var/log/centreon-engine/archives \
          /usr/share/centreon/lib/centreon-broker
chmod 0755 /var/spool/centreon
chmod 0755 /var/lib/centreon-broker
chmod 0755 /var/lib/centreon-gorgone
EOF

COPY --chown=centreon-engine:centreon-engine <<EOF /etc/centreon-engine/engine-context.json
{"app_secret":"","salt":""}
EOF

# Copy runtime files from extractor in bulk
# Batch copy binaries, libraries, configs, and modules to reduce layers and cherry-picking
COPY --from=extractor --link --chmod=755 \
    /extracted/usr/sbin/ /usr/sbin/

COPY --from=extractor --link --chmod=755 \
    /extracted/usr/lib*/ /usr/lib*/

COPY --from=extractor --link --chmod=755 \
    /extracted/usr/share/centreon/ /usr/share/centreon/

# Configuration files (--chown prevents --link)
COPY --from=extractor --chown=centreon-engine:centreon-engine --chmod=664 \
     /extracted/etc/ /etc/

# Copy entrypoint and sudoers (from source repo, not from .deb packages)
COPY --chmod=755 --chown=centreon-engine:centreon-engine \
     ./.github/docker/centreon-engine/trixie/entrypoint /var/lib/centreon-engine

COPY --chmod=600 ./.github/docker/centreon-engine/sudoersCentreon /etc/sudoers.d/centreon

# Verify critical runtime files
RUN test -f /usr/sbin/centengine && \
    test -f /usr/lib*/libcentreon_clib.so && \
    test -d /etc/centreon-engine/

# OCI image metadata for supply chain traceability (used by Scout, registries, and auditing)
ARG BUILD_DATE
ARG BUILD_VERSION=unknown
ARG PACKAGE_SOURCES=unknown

LABEL org.opencontainers.image.title="Centreon Engine" \
      org.opencontainers.image.description="Centreon Engine monitoring scheduler/executor" \
      org.opencontainers.image.created="${BUILD_DATE}" \
      org.opencontainers.image.version="${BUILD_VERSION}" \
      org.opencontainers.image.vendor="Centreon" \
      org.centreon.package-sources="${PACKAGE_SOURCES}" \
      org.centreon.base-image="debian:13-slim"

# gRPC engine management API (inter-container, no host mapping needed)
EXPOSE 50155

WORKDIR /var/lib/centreon-engine

USER centreon-engine

# /tmp/docker.ready is touched by 99-logs.sh once every container.d/*.sh
# entrypoint script has run, right before centengine itself is exec'd.
HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=3 \
    CMD test -f /tmp/docker.ready || exit 1

ENTRYPOINT ["/var/lib/centreon-engine/container.sh"]

CMD ["/usr/sbin/centengine", "/etc/centreon-engine/centengine.cfg"]
