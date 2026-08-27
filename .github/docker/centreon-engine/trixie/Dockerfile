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
FROM debian:13-slim AS runtime

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
useradd -u 900 -g centreon -m -r -l -d /var/spool/centreon -c "Centreon" -s /bin/bash centreon
groupadd -g 901 centreon-engine
useradd -u 901 -g centreon-engine -m -r -l -d /var/lib/centreon-engine -c "Centreon Engine" -s /bin/bash centreon-engine
groupadd -g 902 centreon-broker
useradd -u 902 -g centreon-broker -m -r -l -d /var/lib/centreon-broker -c "Centreon Broker" -s /bin/bash centreon-broker
groupadd -g 903 centreon-gorgone
useradd -u 903 -g centreon-gorgone -m -r -l -d /var/lib/centreon-gorgone -c "Centreon Gorgone" -s /bin/bash centreon-gorgone
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

# Fetch the apt-plugins-stable GPG key/repo and install runtime dependencies
# in a single layer. gnupg/wget are only needed transiently to bootstrap the
# repo, so they're installed and removed within this same RUN — keeping
# install+removal in one layer means their bytes never persist in the final
# image. A previous version split this across two RUNs, installing gnupg/wget
# in one layer and only purging them in a later one; that doesn't shrink the
# earlier layer at all (overlay layers are additive), so their full install
# footprint stayed baked into the image regardless of the later purge.
#
# apt-plugins-stable now publishes arm64 .deb packages for libnet-curl-perl,
# libcrypt-openssl-aes-perl and libssh-session-perl, so the CPAN compilation
# fallback (and the build-essential/dev headers it required) is no longer needed.
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    bash -eo pipefail <<'EOF'
apt-get update
apt-get install -y --no-install-recommends gnupg wget ca-certificates
wget -qO- https://apt-key.centreon.com | gpg --dearmor > /etc/apt/trusted.gpg.d/centreon.gpg
echo "deb [arch=amd64,arm64] https://packages.centreon.com/apt-plugins-stable/ trixie main" > /etc/apt/sources.list.d/centreon-plugins.list
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
# Batch copy binaries, libraries and modules to reduce layers and cherry-picking.
# NOTE: a single non-wildcard destination is required here — Docker does not
# glob-expand COPY destinations, so a previous "/usr/lib*/ /usr/lib*/" form
# literally created a directory named "lib*" instead of landing files in
# /usr/lib, /usr/lib64 or the real multiarch lib dir, which the dynamic linker
# never searches (centengine failed at runtime with "cannot open shared
# object file", while the path-guessing verify RUN below still "passed"
# because its shell glob matched that same bogus literal directory).
COPY --from=extractor --link --chmod=755 \
    /extracted/usr/ /usr/

# Configuration files (--chown prevents --link)
COPY --from=extractor --chown=centreon-engine:centreon-engine --chmod=664 \
     /extracted/etc/ /etc/

# COPY --chmod applies the same mode to every copied entry, files and
# directories alike. The 664 above (no execute bit) landed on
# /etc/centreon-engine and /etc/centreon-engine/conf.d too, since the .deb
# package ships those as real directory entries under /extracted/etc/ — that
# silently clobbered the 775 already set on them above, making the
# directories non-traversable and centengine.cfg unreadable
# ("Permission denied") even though the file itself was still mode 664.
RUN chmod 0775 /etc/centreon-engine /etc/centreon-engine/conf.d /etc/centreon-broker

# Copy entrypoint and sudoers (from source repo, not from .deb packages)
COPY --chmod=755 --chown=centreon-engine:centreon-engine \
     ./.github/docker/centreon-engine/trixie/entrypoint /var/lib/centreon-engine

COPY --chmod=600 ./.github/docker/centreon-engine/sudoersCentreon /etc/sudoers.d/centreon

# Verify critical runtime files. Checking the dynamic linker can actually
# resolve centengine's shared libs (rather than guessing their path with a
# glob) is what would have caught the /usr/lib* destination bug above at
# build time instead of ~1 minute into the boot-test job. bash -eo pipefail
# so a failing `ldd` isn't masked by grep's own (unrelated) exit status.
RUN bash -eo pipefail <<'EOF'
test -f /usr/sbin/centengine
test -d /etc/centreon-engine/
! ldd /usr/sbin/centengine | grep -q "not found"
EOF

# OCI image metadata for supply chain traceability (used by Scout, registries, and auditing)
ARG BUILD_DATE
ARG BUILD_VERSION=unknown
ARG PACKAGE_SOURCES=unknown
ARG GIT_REVISION=unknown

LABEL org.opencontainers.image.title="Centreon Engine" \
      org.opencontainers.image.description="Centreon Engine monitoring scheduler/executor" \
      org.opencontainers.image.created="${BUILD_DATE}" \
      org.opencontainers.image.version="${BUILD_VERSION}" \
      org.opencontainers.image.revision="${GIT_REVISION}" \
      org.opencontainers.image.source="https://github.com/centreon/centreon-collect" \
      org.opencontainers.image.licenses="Apache-2.0" \
      org.opencontainers.image.vendor="Centreon" \
      org.centreon.package-sources="${PACKAGE_SOURCES}" \
      org.centreon.base-image="debian:13-slim" \
      org.centreon.release-stage="beta"

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
