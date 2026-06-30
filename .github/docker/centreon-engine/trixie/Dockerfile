# syntax=docker/dockerfile:1

#############################################
# Stage 1: Extract files from .deb packages
#############################################
FROM debian:13-slim AS extractor

ARG TARGETARCH

RUN apt-get update && apt-get install -y --no-install-recommends \
    dpkg \
    && rm -rf /var/lib/apt/lists/*

# Extract .deb packages (mounted via --mount during build)
# Engine installs all except broker-cbd (line 90 of original Dockerfile)
RUN --mount=type=bind,source=packages-centreon,target=/packages \
    mkdir -p /extracted && \
    cd /packages/${TARGETARCH} && \
    for deb in centreon*.deb; do \
        # Skip broker-cbd package (not needed for engine)
        if [[ "$deb" == *"broker-cbd"* ]]; then \
            echo "Skipping $deb (not needed for engine)"; \
            continue; \
        fi; \
        echo "Extracting $deb..."; \
        dpkg-deb -x "$deb" /extracted/; \
    done && \
    echo "=== Extracted binaries ===" && \
    ls -lah /extracted/usr/sbin/ || true && \
    echo "=== Extracted libraries ===" && \
    ls -lah /extracted/usr/lib*/ || true && \
    echo "=== Extracted engine modules ===" && \
    ls -lah /extracted/usr/lib*/centreon-engine/ || true && \
    echo "=== Extracted broker module ===" && \
    ls -lah /extracted/usr/lib*/cbmod.so || true && \
    echo "=== Extracted configs ===" && \
    ls -lah /extracted/etc/centreon-engine/ || true && \
    ls -lah /extracted/etc/centreon-broker/ || true

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
wget -O- https://apt-key.centreon.com | gpg --dearmor \
    | tee /etc/apt/trusted.gpg.d/centreon.gpg > /dev/null 2>&1
echo "deb [arch=amd64] https://packages.centreon.com/apt-plugins-stable/ trixie main" \
    > /etc/apt/sources.list.d/centreon-plugins.list
EOF

# amd64: install pre-built Centreon packages.
# arm64: build from CPAN (official Debian arm64 packages exist but at version 0.8,
#        below the >= 1.1 requirement of centreon-plugin-*); create equivs shims so
#        APT dependency resolution passes when the plugin is installed later.
# TODO: remove arm64 workaround once perl-cpan-libraries delivers trixie-arm64
#       packages to apt-plugins-stable (tracked in MON-198411 / centreon-plugins).
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    --mount=type=cache,target=/root/.cpanm \
    bash -e <<'EOF'
apt-get update
ARCH=$(dpkg --print-architecture)
if [ "$ARCH" = "amd64" ]; then
    apt-get install -y --no-install-recommends \
        libnet-curl-perl \
        libcrypt-openssl-aes-perl \
        libssh-session-perl
else
    apt-get install -y --no-install-recommends \
        cpanminus equivs build-essential \
        libcurl4-openssl-dev libssl-dev libssh-dev pkg-config
    cpanm --notest Net::Curl Crypt::OpenSSL::AES Libssh::Session
    cat > /tmp/centreon-arm64-perl-shims <<'CTRL'
Section: perl
Priority: optional
Standards-Version: 3.9.2

Package: centreon-arm64-perl-shims
Version: 1.1-1
Provides: libnet-curl-perl (= 1.1), libcrypt-openssl-aes-perl (= 1.1), libssh-session-perl (= 1.1)
Description: APT shims for Centreon Perl deps installed via cpanm
CTRL
    cd /tmp && equivs-build centreon-arm64-perl-shims
    dpkg -i /tmp/centreon-arm64-perl-shims*.deb
    apt-get remove -y --purge cpanminus equivs build-essential \
        libcurl4-openssl-dev libssl-dev libssh-dev pkg-config
    apt-get autoremove -y
    rm -rf /tmp/centreon-arm64-perl-shims*
fi
apt-get install -y --no-install-recommends \
    python3 \
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
apt-get remove -y --purge libcap2-bin
apt-get autoremove -y
rm -rf /tmp/* /var/tmp/* /usr/share/doc/* /usr/share/man/*
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
         /usr/lib64/nagios
ln -s /usr/lib/nagios/plugins /usr/lib64/nagios/plugins
chown -R centreon-engine:centreon-engine \
         /etc/centreon-engine \
         /var/lib/centreon-engine \
         /var/log/centreon-engine \
         /var/lib/centreon/centplugins
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

# Copy ONLY runtime files from extractor

# Binaries
COPY --from=extractor --link --chmod=755 /extracted/usr/sbin/centengine /usr/sbin/centengine

# Core library
COPY --from=extractor --link --chmod=755 /extracted/usr/lib*/libcentreon_clib.so /usr/lib/libcentreon_clib.so

# Protobuf shared libraries (centreon-protobuf-lib package)
COPY --from=extractor --link --chmod=755 /extracted/usr/lib*/libcentreon_engine_conf.so /usr/lib/libcentreon_engine_conf.so
COPY --from=extractor --link --chmod=755 /extracted/usr/lib*/libcentreon_pb_bbdo.so /usr/lib/libcentreon_pb_bbdo.so

# Broker module (cbmod.so) - engine loads this
COPY --from=extractor --link --chmod=755 /extracted/usr/lib*/cbmod.so /usr/lib/cbmod.so

# Broker modules (cbmod.so needs these!)
COPY --from=extractor --link --chmod=755 /extracted/usr/share/centreon/lib/centreon-broker/*.so /usr/share/centreon/lib/centreon-broker/

# Engine modules (directory already created above)
COPY --from=extractor --link --chmod=755 /extracted/usr/lib64/centreon-engine/*.so /usr/lib64/centreon-engine/

# Connector binaries
COPY --from=extractor --link --chmod=755 /extracted/usr/lib64/centreon-connector/ /usr/lib64/centreon-connector/

# Configuration files (--chown prevents --link)
COPY --from=extractor --chown=centreon-engine:centreon-engine --chmod=664 \
     /extracted/etc/centreon-engine/ /etc/centreon-engine/

# Copy entrypoint and sudoers (from source repo, not from .deb packages)
COPY --chmod=755 --chown=centreon-engine:centreon-engine \
     ./.github/docker/centreon-engine/trixie/entrypoint /var/lib/centreon-engine

COPY --chmod=600 ./.github/docker/centreon-engine/sudoersCentreon /etc/sudoers.d/centreon

# Verify all critical files exist
RUN echo "=== Verification ===" && \
    ls -lah /usr/sbin/centengine && \
    ls -lah /usr/lib/libcentreon_clib.so && \
    ls -lah /usr/lib/libcentreon_engine_conf.so && \
    ls -lah /usr/lib/libcentreon_pb_bbdo.so && \
    ls -lah /usr/lib/cbmod.so && \
    ls -lah /usr/lib64/centreon-engine/*.so && \
    ls -lah /usr/lib64/centreon-connector/ && \
    ls -lah /etc/centreon-engine/*.cfg && \
    echo "=== Files verified ===" && \
    echo "Note: Binary version check skipped during build (will work at runtime)"

# gRPC engine management API (inter-container, no host mapping needed)
EXPOSE 50155

WORKDIR /var/lib/centreon-engine

USER centreon-engine

ENTRYPOINT ["/var/lib/centreon-engine/container.sh"]

CMD ["/usr/sbin/centengine", "/etc/centreon-engine/centengine.cfg"]
