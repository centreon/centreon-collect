# syntax=docker/dockerfile:1

#############################################
# Runtime - Minimal final image
#############################################
# No extractor stage: the runtime payload (binaries, libs, modules, configs)
# is staged straight from the nfpm manifests by package-collect.yml's "Stage
# centreon-broker package-less Docker payload" step
# (.github/scripts/manifest_stage.py), which replays their contents: entries
# against build/ - the same files nfpm itself would pack, no .deb ever
# built or extracted for this image. collect.yml's dockerize-engine-broker
# job restores that staged tree per-arch into staging-broker/{amd64,arm64}/
# before this Dockerfile builds.
FROM debian:13-slim AS runtime

ARG TARGETARCH
ARG VERSION
ARG IS_CLOUD
ARG STABILITY

ENV DEBIAN_FRONTEND=noninteractive

# Create users and groups (must match current setup for compatibility)
RUN bash -e <<EOF
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
usermod -aG centreon-gorgone centreon-broker
usermod -aG centreon centreon-broker
usermod -aG sudo centreon-broker
EOF

# Configure APT for optimal container usage
COPY <<EOF /etc/apt/apt.conf.d/99custom
Acquire::Retries "10";
Acquire::https::Timeout "300";
Acquire::http::Timeout "300";
APT::Install-Recommends "false";
APT::Install-Suggests "false";
EOF

# Install ONLY runtime dependencies (no -dev packages, no package overhead)
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    bash -e <<EOF
apt-get update
apt-get install -y --no-install-recommends \
    libmariadb3 \
    librrd8t64 \
    libgnutls30t64 \
    liblua5.4-0 \
    libssl3 \
    libssh2-1t64 \
    libcurl4t64 \
    zlib1g \
    sudo \
    ca-certificates
rm -rf /tmp/* /var/tmp/* /usr/share/doc/* /usr/share/man/* /usr/share/info/* /usr/share/locale/* /var/cache/apt/* /var/log/*
EOF

# Create directory structure
RUN bash -e <<EOF
mkdir -p /etc/centreon-broker \
         /var/lib/centreon-broker \
         /var/log/centreon-broker \
         /usr/share/centreon/lib/centreon-broker \
         /usr/share/centreon-broker/lua
chown -R centreon-broker:centreon-broker \
         /etc/centreon-broker \
         /var/lib/centreon-broker \
         /var/log/centreon-broker \
         /usr/share/centreon-broker
chmod 775 /etc/centreon-broker \
          /var/lib/centreon-broker \
          /var/log/centreon-broker \
          /usr/share/centreon-broker \
          /usr/share/centreon-broker/lua
chmod 0755 /var/spool/centreon
chmod 0755 /var/lib/centreon-engine
chmod 0755 /var/lib/centreon-gorgone
EOF

# Copy the staged runtime payload in bulk (binaries, libraries, modules).
# NOTE: a single non-wildcard destination is required here — Docker does not
# glob-expand COPY destinations. A per-file cherry-pick was used previously,
# which happened to be safe on its own, but diverged from the same bulk-copy
# approach used on the sibling centreon-engine Dockerfile after that one hit
# a real bug (a wildcard *destination* like "/usr/lib*/ /usr/lib*/" silently
# created a literal directory named "lib*"). Aligning both Dockerfiles on the
# same non-wildcard whole-tree copy — safe here because the staged manifest
# set (see package-collect.yml) only includes clib/protobuf-lib/broker*, so
# nothing engine/connector-specific ends up in the staged tree to begin with.
COPY --link --chmod=755 \
    staging-broker/${TARGETARCH}/usr/ /usr/

# Configuration files (--chown prevents --link)
COPY --chown=centreon-broker:centreon-broker --chmod=664 \
     staging-broker/${TARGETARCH}/etc/ /etc/

# COPY --chmod applies the same mode to every copied entry, files and
# directories alike — if the .deb package ever ships a subdirectory directly
# under /etc/centreon-broker/, the 664 above would clobber the 775 already
# set on it, making it non-traversable (this exact bug broke centengine.cfg
# on the sibling centreon-engine image). Re-asserted defensively here since
# broker has no boot-test/config-wiring-test in CI to catch it if it recurs.
RUN chmod 0775 /etc/centreon-broker

# Copy entrypoint scripts (from source repo, not from .deb packages)
COPY --chmod=755 --chown=centreon-broker:centreon-broker \
     ./.github/docker/centreon-broker/trixie/entrypoint /var/lib/centreon-broker

# Verify critical runtime files. Checking the dynamic linker can actually
# resolve cbd/cbwd's shared libs (rather than guessing libcentreon_clib.so's
# exact path with a glob) is what caught the equivalent bug on the sibling
# centreon-engine Dockerfile — a bad copy path can pass a file-existence
# check while still being unresolvable at runtime.
RUN bash -eo pipefail <<'EOF'
test -f /usr/sbin/cbd
test -f /usr/sbin/cbwd
ls /usr/share/centreon/lib/centreon-broker/*.so >/dev/null
ls /etc/centreon-broker/*.json >/dev/null
! ldd /usr/sbin/cbd | grep -q "not found"
! ldd /usr/sbin/cbwd | grep -q "not found"
EOF

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
      org.centreon.base-image="debian:13-slim" \
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
CMD ["/bin/bash"]
