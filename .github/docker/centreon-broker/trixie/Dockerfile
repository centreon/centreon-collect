#############################################
# Stage 1: Extract files from .deb packages
#############################################
FROM debian:13-slim AS extractor

ARG TARGETARCH

RUN apt-get update && apt-get install -y --no-install-recommends \
    dpkg \
    && rm -rf /var/lib/apt/lists/*

# Extract .deb packages (mounted via --mount during build)
RUN --mount=type=bind,source=packages-centreon,target=/packages \
    mkdir -p /extracted && \
    cd /packages/${TARGETARCH} && \
    for deb in *.deb; do \
        echo "Extracting $deb..."; \
        dpkg-deb -x "$deb" /extracted/; \
    done && \
    echo "=== Extracted binaries ===" && \
    ls -lah /extracted/usr/sbin/ || true && \
    echo "=== Extracted libraries ===" && \
    ls -lah /extracted/usr/lib/ || true && \
    echo "=== Extracted modules ===" && \
    ls -lah /extracted/usr/share/centreon/lib/centreon-broker/ || true && \
    echo "=== Extracted configs ===" && \
    ls -lah /extracted/etc/centreon-broker/ || true

#############################################
# Stage 2: Runtime - Minimal final image
#############################################
FROM debian:13-slim

ARG VERSION
ARG IS_CLOUD
ARG STABILITY

ENV DEBIAN_FRONTEND=noninteractive

# Create users and groups (must match current setup for compatibility)
RUN groupadd -g 900 centreon && \
    useradd -u 900 -g centreon -m -r -d /var/spool/centreon -c "Centreon" -s /bin/bash centreon && \
    groupadd -g 901 centreon-engine && \
    useradd -u 901 -g centreon-engine -m -r -d /var/lib/centreon-engine -c "Centreon Engine" -s /bin/bash centreon-engine && \
    groupadd -g 902 centreon-broker && \
    useradd -u 902 -g centreon-broker -m -r -d /var/lib/centreon-broker -c "Centreon Broker" -s /bin/bash centreon-broker && \
    groupadd -g 903 centreon-gorgone && \
    useradd -u 903 -g centreon-gorgone -m -r -d /var/lib/centreon-gorgone -c "Centreon Gorgone" -s /bin/bash centreon-gorgone && \
    usermod -aG centreon-broker centreon-engine && \
    usermod -aG centreon-engine centreon-broker && \
    usermod -aG centreon-gorgone centreon-broker && \
    usermod -aG centreon centreon-broker && \
    usermod -aG sudo centreon-broker

# Configure APT for optimal container usage
RUN echo 'Acquire::Retries "10";\n\
Acquire::https::Timeout "300";\n\
Acquire::http::Timeout "300";\n\
APT::Install-Recommends "false";\n\
APT::Install-Suggests "false";' > /etc/apt/apt.conf.d/99custom

# Install ONLY runtime dependencies (no -dev packages, no package overhead)
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3-minimal \
    python3-venv \
    libmariadb3 \
    librrd8t64 \
    libgnutls30t64 \
    liblua5.3-0 \
    libssl3 \
    libssh2-1t64 \
    libcurl4t64 \
    zlib1g \
    sudo \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
               /tmp/* \
               /var/tmp/* \
               /usr/share/doc/* \
               /usr/share/man/* \
               /var/cache/apt/*

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
    chmod 0755 /var/spool/centreon && \
    chmod 0755 /var/lib/centreon-engine && \
    chmod 0755 /var/lib/centreon-gorgone

# Copy ONLY runtime files from extractor (no package metadata, docs, or systemd files)
COPY --from=extractor --chmod=755 /extracted/usr/sbin/cbd /usr/sbin/cbd
COPY --from=extractor --chmod=755 /extracted/usr/sbin/cbwd /usr/sbin/cbwd

# Copy core library - handle both /usr/lib and /usr/lib64 locations
COPY --from=extractor --chmod=755 /extracted/usr/lib*/libcentreon_clib.so /usr/lib/libcentreon_clib.so

# Copy all broker modules
COPY --from=extractor --chmod=755 /extracted/usr/share/centreon/lib/centreon-broker/*.so /usr/share/centreon/lib/centreon-broker/

# Copy configuration files with correct ownership
COPY --from=extractor --chown=centreon-broker:centreon-broker --chmod=664 \
     /extracted/etc/centreon-broker/ /etc/centreon-broker/

# Copy entrypoint scripts (from source repo, not from .deb packages)
COPY --chmod=755 --chown=centreon-broker:centreon-broker \
     ./.github/docker/centreon-broker/trixie/entrypoint /var/lib/centreon-broker

# Verify all critical files exist
RUN echo "=== Verification ===" && \
    ls -lah /usr/sbin/cbd /usr/sbin/cbwd && \
    ls -lah /usr/lib/libcentreon_clib.so && \
    ls -lah /usr/share/centreon/lib/centreon-broker/*.so && \
    ls -lah /etc/centreon-broker/*.json && \
    echo "=== Files verified ===" && \
    echo "Note: Binary version check skipped during build (will work at runtime)"

# Pre-install Python API dependencies at build time (10-15s startup improvement)
RUN python3 -m venv /opt/centreon/venv && \
    /opt/centreon/venv/bin/pip install --no-cache-dir --upgrade pip && \
    /opt/centreon/venv/bin/pip install --no-cache-dir \
        fastapi==0.115.0 \
        uvicorn==0.32.0 && \
    chown -R centreon-broker:centreon-broker /opt/centreon/venv

ENV PATH="/opt/centreon/venv/bin:$PATH"

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

ENTRYPOINT ["/var/lib/centreon-broker/container.sh"]

# TODO: CMD depend on restart/reload management (grpc) TBD later
CMD ["/bin/bash"]
