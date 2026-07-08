# syntax=docker/dockerfile:1

# Build arg to select gorgone Perl source: false=use .deb (production), true=use local repo (dev/test)
ARG USE_SOURCE_GORGONE=false

#############################################
# Stage 0: Build grpcurl from source (always uses an up-to-date Go
# toolchain, unlike fullstorydev's precompiled release tarballs which
# stay pinned to whatever Go version was current when they were cut)
#############################################
FROM golang:1-trixie AS grpcurl-builder
RUN go install github.com/fullstorydev/grpcurl/cmd/grpcurl@v1.9.3

#############################################
# Stage 1: Extract files from .deb packages
#############################################
FROM debian:13-slim AS extractor

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    bash -e <<EOF
apt-get update
apt-get install -y --no-install-recommends dpkg
EOF

# Extract .deb packages (mounted via --mount during build)
RUN --mount=type=bind,source=packages-centreon,target=/packages \
    mkdir -p /extracted && \
    cd /packages && \
    for deb in centreon-gorgone*.deb centreon-perl*.deb; do \
        echo "Extracting $deb..."; \
        dpkg-deb -x "$deb" /extracted/; \
    done && \
    echo "=== Extracted binaries ===" && \
    ls -lah /extracted/usr/bin/ || true && \
    echo "=== Extracted Perl modules ===" && \
    ls -lah /extracted/usr/share/perl5/ || true && \
    echo "=== Extracted configs ===" && \
    ls -lah /extracted/etc/centreon-gorgone/ || true

#############################################
# Stage 1b/1c: Gorgone Perl source selector
# USE_SOURCE_GORGONE=false → extract from .deb (production default)
# USE_SOURCE_GORGONE=true  → copy from local repo source (dev/testing)
#############################################
FROM scratch AS gorgone-perl-false
COPY --from=extractor /extracted/usr/share/perl5/gorgone /gorgone-modules/

FROM scratch AS gorgone-perl-true
COPY gorgone/gorgone/ /gorgone-modules/

ARG USE_SOURCE_GORGONE=false
# hadolint ignore=DL3006
FROM gorgone-perl-${USE_SOURCE_GORGONE} AS gorgone-perl-selected

#############################################
# Stage 1d/1e: Centreon Perl libs selector
#############################################
FROM scratch AS centreon-perl-false
COPY --from=extractor /extracted/usr/share/perl5/centreon /centreon-modules/

FROM scratch AS centreon-perl-true
COPY perl-libs/lib/centreon/ /centreon-modules/

ARG USE_SOURCE_GORGONE=false
# hadolint ignore=DL3006
FROM centreon-perl-${USE_SOURCE_GORGONE} AS centreon-perl-selected

#############################################
# Stage 1f/1g: Gorgone config selector
#############################################
FROM scratch AS gorgone-config-false
COPY --from=extractor /extracted/etc/centreon-gorgone/ /gorgone-config/

FROM scratch AS gorgone-config-true
COPY gorgone/packaging/configuration/config.yaml /gorgone-config/config.yaml
COPY gorgone/packaging/configuration/action.yaml /gorgone-config/config.d/39-action.yaml
COPY gorgone/packaging/configuration/whitelist.conf.d/ /gorgone-config/config.d/whitelist.conf.d/

ARG USE_SOURCE_GORGONE=false
# hadolint ignore=DL3006
FROM gorgone-config-${USE_SOURCE_GORGONE} AS gorgone-config-selected

#############################################
# Stage 2: Runtime - Debian 13 slim image
#############################################
FROM debian:13-slim AS gorgone

# Documented image contract — values are normally injected via docker compose / -e flags.
# Declared here so `docker inspect` lists them and `docker exec -it ... bash` sees them.
ENV TYPE= \
    NAME= \
    GORGONE_UID= \
    GORGONE_TOKEN= \
    GORGONE_SSL= \
    CENTRAL_HOST= \
    CENTRAL_PORT= \
    APP_SECRET= \
    SALT=

ARG VERSION
ARG IS_CLOUD
ARG STABILITY

# Create users and groups (Debian style)
RUN bash -e <<EOF
groupadd -g 33 www-data 2>/dev/null || true
groupadd -g 900 centreon
useradd -u 900 -g centreon -m -r -d /var/spool/centreon -s /bin/bash centreon
groupadd -g 901 centreon-engine
useradd -u 901 -g centreon-engine -m -r -d /var/lib/centreon-engine -s /bin/bash centreon-engine
groupadd -g 902 centreon-broker
useradd -u 902 -g centreon-broker -m -r -d /var/lib/centreon-broker -s /bin/bash centreon-broker
groupadd -g 903 centreon-gorgone
useradd -u 903 -g centreon-gorgone -m -r -d /var/lib/centreon-gorgone -s /bin/bash centreon-gorgone
usermod -aG centreon centreon-gorgone
usermod -aG www-data centreon-gorgone
usermod -aG centreon-engine centreon-gorgone
usermod -aG centreon-broker centreon-gorgone
EOF

# Configure APT for optimal container usage
COPY <<EOF /etc/apt/apt.conf.d/99custom
Acquire::Retries "10";
Acquire::https::Timeout "300";
Acquire::http::Timeout "300";
APT::Install-Recommends "false";
APT::Install-Suggests "false";
EOF

# Add Centreon plugins-stable repository (provides pre-built Perl modules for amd64 and arm64)
# and install those packages plus runtime dependencies
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    bash -e <<'EOF'
apt-get update
apt-get install -y --no-install-recommends gnupg wget ca-certificates
echo "deb [arch=amd64,arm64] https://packages.centreon.com/apt-plugins-stable/ trixie main" \
    > /etc/apt/sources.list.d/centreon-plugins.list
wget -O- https://apt-key.centreon.com | gpg --dearmor \
    | tee /etc/apt/trusted.gpg.d/centreon.gpg > /dev/null 2>&1
apt-get update
apt-get install -y --no-install-recommends \
    libnet-curl-perl \
    libcrypt-openssl-aes-perl \
    libssh-session-perl \
    libmojo-ioloop-signal-perl \
    perl \
    libdbi-perl \
    libdbd-mysql-perl \
    libdbd-sqlite3-perl \
    libjson-xs-perl \
    libyaml-libyaml-perl \
    libxml-simple-perl \
    libhttp-daemon-perl \
    libdigest-md5-file-perl \
    libschedule-cron-perl \
    librrds-perl \
    libhash-merge-perl \
    libjson-perl \
    libmojolicious-perl \
    libio-socket-ssl-perl \
    libev-perl \
    libclass-dbi-perl \
    libcryptx-perl \
    libcrypt-urandom-perl \
    libnet-ssleay-perl \
    libzmq-ffi-perl \
    libzmq-constants-perl \
    rrdtool \
    curl \
    openssl \
    libssh2-1t64 \
    libzmq5 \
    libmariadb3 \
    libgnutls30t64 \
    sudo \
    lsb-release \
    iputils-ping \
    wget \
    tzdata \
    ca-certificates \
    libnetaddr-ip-perl
rm -rf /tmp/* /var/tmp/* /usr/share/doc/* /usr/share/man/*
EOF

# Create directory structure
RUN bash -e <<EOF
mkdir -p /etc/centreon/config.d/ \
         /etc/centreon-gorgone/config.d \
         /var/cache/centreon/ \
         /var/lib/centreon/centcore/ \
         /var/lib/centreon-gorgone/.keys \
         /var/log/centreon-gorgone \
         /var/lib/centreon-engine/rw \
         /usr/local/lib/centreon-gorgone
chown -R centreon-gorgone:centreon /etc/centreon/
chmod -R 775 /etc/centreon/
chown -R centreon-gorgone:centreon-gorgone /etc/centreon-gorgone/
chmod -R 775 /etc/centreon-gorgone/
chown centreon:centreon /var/cache/centreon
chmod 775 /var/cache/centreon
touch /var/lib/centreon/centcore.cmd
chown -R centreon-gorgone:centreon /var/lib/centreon/
chmod -R 775 /var/lib/centreon/
chown -R centreon-engine:centreon-engine /var/lib/centreon-engine
chmod 0755 /var/lib/centreon-engine
chmod 775 /var/lib/centreon-engine/rw
chown -R centreon-gorgone:centreon-gorgone \
          /var/lib/centreon-gorgone \
          /var/log/centreon-gorgone
chmod 775 /var/lib/centreon-gorgone /var/log/centreon-gorgone
chmod 755 /var/lib/centreon-gorgone/.keys
chmod 0755 /var/spool/centreon
chmod 0755 /var/lib/centreon-broker
rm -f /bin/systemctl
EOF

# Copy ONLY runtime files from extractor (no package metadata, docs, or systemd files)

# Gorgone binaries
COPY --from=extractor --link --chmod=755 /extracted/usr/bin/gorgoned /usr/bin/gorgoned

# Perl modules (centreon-gorgone and centreon-perl) - use Debian's perl5 path
# gorgone modules come from gorgone-perl-selected (either .deb or local repo source)
COPY --from=gorgone-perl-selected --link /gorgone-modules/ /usr/share/perl5/gorgone/
COPY --from=centreon-perl-selected --link /centreon-modules/ /usr/share/perl5/centreon/

# Configuration files
COPY --from=gorgone-config-selected --chown=centreon-gorgone:centreon-gorgone --chmod=775 \
     /gorgone-config/ /etc/centreon-gorgone/

# Static poller config — dynamic params (id, token, address, port) are injected via env vars
# (GORGONE_UID, GORGONE_TOKEN, CENTRAL_HOST, CENTRAL_PORT) using load_env_config at startup
COPY --chown=centreon-gorgone:centreon-gorgone --chmod=775 \
     .github/docker/centreon-gorgone/trixie/config/40-gorgoned.yaml \
     /etc/centreon-gorgone/config.d/40-gorgoned.yaml

# Install grpcurl for gRPC-based centengine management (replaces HTTP api_control calls)
# Binary comes from the grpcurl-builder stage, compiled from source with an
# up-to-date Go toolchain instead of fullstorydev's stale precompiled release
COPY --from=grpcurl-builder --link --chmod=755 /go/bin/grpcurl /usr/local/bin/grpcurl
RUN mkdir -p /usr/share/centreon-engine/proto

# Copy engine proto definitions for grpcurl (no reflection API on centengine)
COPY --link --chmod=644 engine/enginerpc/engine.proto /usr/share/centreon-engine/proto/engine.proto
COPY --link --chmod=644 common/process_stat/process_stat.proto /usr/share/centreon-engine/proto/process_stat.proto

# Copy entrypoint, systemctl, and sudoers (from source repo, not from .deb packages)
COPY --link --chmod=755 .github/docker/centreon-gorgone/trixie/systemctl /usr/bin/systemctl
COPY --link --chmod=755 .github/docker/centreon-gorgone/trixie/entrypoint /usr/local/lib/centreon-gorgone/
COPY --link --chmod=600 .github/docker/centreon-gorgone/trixie/sudoers/centreon /etc/sudoers.d/centreon
COPY --link --chmod=600 gorgone/packaging/sudoers.d/centreon-gorgone /etc/sudoers.d/centreon-gorgone
COPY --link --chmod=600 .github/docker/centreon-gorgone/trixie/sudoers/sudoersCentreon /etc/sudoers.d/centreon-docker
COPY --link --chmod=755 gorgone/contrib/gorgone_install_plugins.pl /usr/bin/gorgone_install_plugins.pl

# Perl library path for Debian
ENV PERL5LIB=/usr/share/perl5:/usr/lib/perl5

# Verify critical files exist and Perl syntax
RUN echo "=== Verification ===" && \
    ls -lah /usr/bin/gorgoned && \
    ls -lah /usr/share/perl5/gorgone/ && \
    ls -lah /usr/share/perl5/centreon/ && \
    ls -lah /etc/centreon-gorgone/ && \
    perl -c /usr/bin/gorgoned && \
    echo "=== Files verified ==="

# Set the working directory
WORKDIR /etc/centreon-gorgone/

USER centreon-gorgone

# container.d/99-logs.sh touches this file once every startup script has run.
HEALTHCHECK --interval=10s --timeout=3s --start-period=30s --retries=3 \
    CMD test -f /tmp/docker.ready

ENTRYPOINT ["/usr/local/lib/centreon-gorgone/container.sh"]

CMD ["/usr/bin/perl", "/usr/bin/gorgoned", "--config=/etc/centreon-gorgone/config.yaml", "--severity=debug", "--flushoutput"]
