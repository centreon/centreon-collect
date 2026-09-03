# syntax=docker/dockerfile:1

#############################################
# Stage 1: Extract files from .apk packages
#############################################
FROM alpine:3.20 AS extractor

ARG TARGETARCH

# Extract .apk packages (mounted via --mount during build). apk packages are
# plain tar archives with control/signature tarballs prepended - tar -x on
# the whole file skips them harmlessly and overlays the data payload, same
# effect as dpkg-deb -x on trixie.
# Engine installs all except broker-cbd (not needed for engine).
RUN --mount=type=bind,source=packages-centreon,target=/packages \
    mkdir -p /extracted && \
    cd /packages/${TARGETARCH} && \
    for apk in centreon*.apk; do \
        case "$apk" in \
            *broker-cbd*) continue ;; \
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

ENV GORGONE_UID= \
    NAME= \
    GORGONE_TOKEN= \
    CENTRAL_HOST= \
    APP_SECRET= \
    SALT=

# Create users and groups (must match current setup for compatibility)
RUN addgroup -g 33 www-data 2>/dev/null || true && \
    addgroup -g 900 centreon && \
    adduser -u 900 -G centreon -h /var/spool/centreon -s /bin/sh -D -H centreon && \
    addgroup -g 901 centreon-engine && \
    adduser -u 901 -G centreon-engine -h /var/lib/centreon-engine -s /bin/sh -D -H centreon-engine && \
    addgroup -g 902 centreon-broker && \
    adduser -u 902 -G centreon-broker -h /var/lib/centreon-broker -s /bin/sh -D -H centreon-broker && \
    addgroup -g 903 centreon-gorgone && \
    adduser -u 903 -G centreon-gorgone -h /var/lib/centreon-gorgone -s /bin/sh -D -H centreon-gorgone && \
    addgroup centreon-engine centreon-broker && \
    addgroup centreon-broker centreon-engine && \
    addgroup centreon-engine centreon-gorgone && \
    addgroup centreon-engine centreon && \
    addgroup centreon-engine www-data && \
    mkdir -p /var/lib/centreon-broker /var/lib/centreon-gorgone /var/spool/centreon

# Runtime dependencies, apk-native equivalents of the trixie image's apt list.
#
# KNOWN GAP (documented, not silently dropped): the trixie image additionally
# pulls Centreon's own apt-plugins-stable repo for libnet-curl-perl,
# libcrypt-openssl-aes-perl, libssh-session-perl and
# centreon-plugin-applications-monitoring-centreon-poller, none of which have
# an Alpine/apk equivalent today. Its container.d/40-/41-/45-/46-*.sh scripts
# (dynamic plugin/custom-deps install from plugins.json/custom-deps.json via
# apt) have no apk-repo fallback yet and are intentionally NOT copied into
# this image - see .github/docker/centreon-engine/alpine/entrypoint. Alpine's
# own monitoring-plugins package covers the C-language checks (check_icmp,
# check_snmp) on its own; Perl-based plugins that depend on those 3 missing
# CPAN XS modules will not work until an Alpine apk repo/build path exists
# for them.
#
# check_snmp itself doesn't link net-snmp - it execs the snmpget/snmpbulkget
# CLI tools as a subprocess (confirmed against a real SNMP target: without
# net-snmp-tools it fails with "External command error", exit code 3).
RUN apk add --no-cache \
    monitoring-plugins \
    net-snmp-tools \
    libcap-utils \
    libgcc \
    libstdc++ \
    mariadb-connector-c \
    gnutls \
    lua5.4-libs \
    openssl \
    libssh2 \
    libgcrypt \
    libcurl \
    zlib \
    sudo \
    ca-certificates && \
    setcap cap_net_raw+ep /usr/lib/monitoring-plugins/check_icmp && \
    apk del libcap-utils

# Create directory structure
RUN mkdir -p /etc/centreon-engine \
             /etc/centreon-engine/conf.d \
             /etc/centreon-broker \
             /var/lib/centreon-engine \
             /var/lib/centreon-engine/rw \
             /var/log/centreon-engine \
             /var/log/centreon-engine/archives \
             /var/lib/centreon/centplugins \
             /usr/share/centreon/lib/centreon-broker \
             /usr/lib64/centreon-engine \
             /usr/lib64/nagios \
             /usr/lib/nagios/plugins/custom && \
    ln -s /usr/lib/monitoring-plugins /usr/lib64/nagios/plugins && \
    chown -R centreon-engine:centreon-engine \
             /etc/centreon-engine \
             /var/lib/centreon-engine \
             /var/log/centreon-engine \
             /var/lib/centreon/centplugins \
             /usr/lib/nagios/plugins/custom && \
    chown -R centreon-broker:centreon-broker \
              /etc/centreon-broker && \
    chmod 775 /etc/centreon-engine \
              /etc/centreon-broker \
              /etc/centreon-engine/conf.d \
              /var/lib/centreon-engine \
              /var/lib/centreon-engine/rw && \
    chmod 755 /var/log/centreon-engine \
              /var/log/centreon-engine/archives \
              /usr/share/centreon/lib/centreon-broker && \
    chmod 0755 /var/spool/centreon \
               /var/lib/centreon-broker \
               /var/lib/centreon-gorgone

COPY --chown=centreon-engine:centreon-engine <<EOF /etc/centreon-engine/engine-context.json
{"app_secret":"","salt":""}
EOF

# Copy runtime files from extractor in bulk (see trixie Dockerfile for why a
# single non-wildcard destination is required here).
COPY --from=extractor --link --chmod=755 \
    /extracted/usr/ /usr/

# Configuration files (--chown prevents --link)
COPY --from=extractor --chown=centreon-engine:centreon-engine --chmod=664 \
     /extracted/etc/ /etc/

# COPY --chmod applies the same mode to every copied entry, files and
# directories alike - re-assert 0775 on the config dirs (see trixie
# Dockerfile for the equivalent bug this guards against).
RUN chmod 0775 /etc/centreon-engine /etc/centreon-engine/conf.d /etc/centreon-broker

# Copy entrypoint and sudoers (from source repo, not from .apk packages)
COPY --chmod=755 --chown=centreon-engine:centreon-engine \
     ./.github/docker/centreon-engine/alpine/entrypoint /var/lib/centreon-engine

COPY --chmod=600 ./.github/docker/centreon-engine/sudoersCentreon /etc/sudoers.d/centreon

# Verify critical runtime files, including that the dynamic linker can
# actually resolve centengine's shared libs. musl's ldd/dynamic linker
# reports unresolved libs differently from glibc's ("Error loading shared
# library ...: No such file or directory" rather than "=> not found") -
# match both patterns.
RUN set -e; \
    test -f /usr/sbin/centengine; \
    test -d /etc/centreon-engine/; \
    ! ldd /usr/sbin/centengine 2>&1 | grep -qE "not found|Error loading shared library|Error relocating"

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
      org.centreon.base-image="alpine:3.20" \
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
