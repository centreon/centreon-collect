# syntax=docker/dockerfile:1

#############################################
# Stage 1: Extract files from .apk packages
#############################################
FROM alpine:3.20 AS extractor

ARG TARGETARCH

# Extract .apk packages (mounted via --mount during build). apk packages are
# plain tar archives with a .PKGINFO/control tarball prepended - tar -x on
# the whole file skips the leading control/signature tarballs harmlessly and
# just overlays the data payload, same effect as dpkg-deb -x on trixie.
# NOTE: packages-centreon/ is shared with the sibling centreon-engine build in
# CI and contains engine/connector packages too - skip anything engine/
# connector-specific here, mirroring the trixie extractor's filtering.
RUN --mount=type=bind,source=packages-centreon,target=/packages \
    mkdir -p /extracted && \
    cd /packages/${TARGETARCH} && \
    for apk in *.apk; do \
        case "$apk" in \
            *centreon-engine*|*connector*) continue ;; \
        esac; \
        tar -xzf "$apk" -C /extracted; \
    done && \
    rm -rf /extracted/usr/share/doc/* \
           /extracted/usr/share/man/* \
           /extracted/usr/share/info/* \
           /extracted/usr/share/locale/* \
           /extracted/usr/share/licenses/* \
           /extracted/var/cache/* \
           /extracted/var/log/* \
           /extracted/.PKGINFO \
           /extracted/.SIGN.*

#############################################
# Stage 2: Runtime - Minimal final image
#############################################
FROM alpine:3.20 AS runtime

ARG VERSION
ARG IS_CLOUD
ARG STABILITY

# Create users and groups (must match current setup for compatibility)
RUN addgroup -g 900 centreon && \
    adduser -u 900 -G centreon -h /var/spool/centreon -s /bin/sh -D -H centreon && \
    addgroup -g 901 centreon-engine && \
    adduser -u 901 -G centreon-engine -h /var/lib/centreon-engine -s /bin/sh -D -H centreon-engine && \
    addgroup -g 902 centreon-broker && \
    adduser -u 902 -G centreon-broker -h /var/lib/centreon-broker -s /bin/sh -D -H centreon-broker && \
    addgroup -g 903 centreon-gorgone && \
    adduser -u 903 -G centreon-gorgone -h /var/lib/centreon-gorgone -s /bin/sh -D -H centreon-gorgone && \
    addgroup centreon-broker centreon-engine && \
    addgroup centreon-engine centreon-broker && \
    addgroup centreon-broker centreon-gorgone && \
    addgroup centreon-broker centreon && \
    mkdir -p /var/lib/centreon-engine /var/lib/centreon-gorgone /var/spool/centreon

# Install ONLY runtime dependencies (no -dev packages, no package overhead)
RUN apk add --no-cache \
    python3 \
    py3-pip \
    mariadb-connector-c \
    rrdtool \
    gnutls \
    lua5.4-libs \
    openssl \
    libssh2 \
    libcurl \
    zlib \
    sudo \
    ca-certificates

# Create directory structure
RUN mkdir -p /etc/centreon-broker \
             /var/lib/centreon-broker \
             /var/log/centreon-broker \
             /usr/share/centreon/lib/centreon-broker \
             /usr/share/centreon-broker/lua && \
    chown -R centreon-broker:centreon-broker \
             /etc/centreon-broker \
             /var/lib/centreon-broker \
             /var/log/centreon-broker \
             /usr/share/centreon-broker && \
    chmod 775 /etc/centreon-broker \
              /var/lib/centreon-broker \
              /var/log/centreon-broker \
              /usr/share/centreon-broker \
              /usr/share/centreon-broker/lua && \
    chmod 0755 /var/spool/centreon \
               /var/lib/centreon-engine \
               /var/lib/centreon-gorgone

# Copy runtime files from extractor in bulk (binaries, libraries, modules).
# See the trixie Dockerfile for why this must be a single non-wildcard
# destination - the extractor stage above already excludes engine/connector
# packages, so nothing unrelated ends up in /extracted/usr/ to begin with.
COPY --from=extractor --link --chmod=755 \
    /extracted/usr/ /usr/

# Configuration files (--chown prevents --link)
COPY --from=extractor --chown=centreon-broker:centreon-broker --chmod=664 \
     /extracted/etc/ /etc/

# COPY --chmod applies the same mode to every copied entry, files and
# directories alike - re-assert 0775 on /etc/centreon-broker defensively
# (see trixie Dockerfile for the equivalent bug this guards against).
RUN chmod 0775 /etc/centreon-broker

# Copy entrypoint scripts (from source repo, not from .apk packages)
COPY --chmod=755 --chown=centreon-broker:centreon-broker \
     ./.github/docker/centreon-broker/alpine/entrypoint /var/lib/centreon-broker

# Verify critical runtime files, including that the dynamic linker can
# actually resolve cbd/cbwd's shared libs.
RUN set -e; \
    test -f /usr/sbin/cbd; \
    test -f /usr/sbin/cbwd; \
    ls /usr/share/centreon/lib/centreon-broker/*.so >/dev/null; \
    ls /etc/centreon-broker/*.json >/dev/null; \
    ! ldd /usr/sbin/cbd | grep -q "not found"; \
    ! ldd /usr/sbin/cbwd | grep -q "not found"

# Pre-install Python API dependencies at build time (10-15s startup improvement)
RUN python3 -m venv /opt/centreon/venv && \
    /opt/centreon/venv/bin/pip install --no-cache-dir --upgrade pip && \
    /opt/centreon/venv/bin/pip install --no-cache-dir \
        fastapi==0.140.0 \
        starlette==1.3.1 \
        uvicorn==0.32.0 && \
    chown -R centreon-broker:centreon-broker /opt/centreon/venv

ENV PATH="/opt/centreon/venv/bin:$PATH"

# OCI image metadata for supply chain traceability (used by Scout, registries, and auditing)
ARG BUILD_DATE
ARG BUILD_VERSION=unknown
ARG PACKAGE_SOURCES=unknown
ARG GIT_REVISION=unknown

LABEL org.opencontainers.image.title="Centreon Broker" \
      org.opencontainers.image.description="Centreon Broker event broker (BBDO protocol)" \
      org.opencontainers.image.created="${BUILD_DATE}" \
      org.opencontainers.image.version="${BUILD_VERSION}" \
      org.opencontainers.image.revision="${GIT_REVISION}" \
      org.opencontainers.image.source="https://github.com/centreon/centreon-collect" \
      org.opencontainers.image.licenses="Apache-2.0" \
      org.opencontainers.image.vendor="Centreon" \
      org.centreon.package-sources="${PACKAGE_SOURCES}" \
      org.centreon.base-image="alpine:3.20" \
      org.centreon.release-stage="beta"

# Expose ports (same as current Dockerfile)
# gRPC central-module
EXPOSE 51003
# bbdo central-broker
EXPOSE 5669
# gRPC central-broker
EXPOSE 51001
# bbdo central-rrd
EXPOSE 5670
# gRPC central-rrd
EXPOSE 51002

WORKDIR /var/lib/centreon-broker

USER centreon-broker

# container.sh touches /tmp/docker.ready once startup has completed.
HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=3 \
    CMD test -f /tmp/docker.ready || exit 1

ENTRYPOINT ["/var/lib/centreon-broker/container.sh"]

# TODO: CMD depend on restart/reload management (grpc) TBD later
CMD ["/bin/sh"]
