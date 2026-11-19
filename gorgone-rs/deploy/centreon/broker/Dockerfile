# syntax=docker/dockerfile:1

# centreon-broker image with the victoria_metrics output of this branch, for
# the gorgone-rs proof of concept: it writes metrics and status as described
# in gorgone-rs/docs/victoria-metrics-contract.md (labels, timestamps in
# seconds, rebuild and removal of graphs), so that centreon-web can read its
# graphs from VictoriaMetrics instead of RRD files.
# The build context holds one file (see README.md): `70-victoria_metrics.so`,
# built with the packaging image of the same distribution. The base image
# does not package this module.
# The named build context `entrypoint` (.github/docker/centreon-broker/trixie/
# entrypoint of this checkout) brings the entrypoint without Python: cbd is
# the main process, BROKER_WAIT_FOR_CONFIG waits for a configuration pushed
# by a gorgone-agent (gorgone-rs/docs/containers-contract.md).

ARG BASE_IMAGE=docker.centreon.com/centreon/centreon-broker-trixie:develop
FROM ${BASE_IMAGE} AS rootfs

USER root

# cbd is unchanged: the module embeds http_tsdb and resolves the Broker core
# (global cache, multiplexing) from the cbd of the base image, built from the
# same broker/, common/, bbdo/ and clib/ sources.
COPY --chown=root:root --chmod=755 70-victoria_metrics.so \
     /usr/share/centreon/lib/centreon-broker/70-victoria_metrics.so

# Same mode and owner as the base image (root-owned, not writable by cbd).
# api_control.py of the base image goes with the directory.
RUN rm -rf /usr/local/lib/centreon-broker
COPY --from=entrypoint --chmod=755 . /usr/local/lib/centreon-broker/

# Out: the RRD output and the libraries only it needs (cairo, pango, fonts),
# the legacy SQL outputs replaced by unified_sql, and Python with the venv
# of the former control API. netbase stays (/etc/services, /etc/protocols).
RUN rm -f /usr/share/centreon/lib/centreon-broker/70-rrd.so \
          /usr/share/centreon/lib/centreon-broker/20-storage.so \
          /usr/share/centreon/lib/centreon-broker/80-sql.so \
    && rm -rf /opt/centreon \
    && apt-mark manual netbase >/dev/null \
    && apt-get purge -y --auto-remove python3-minimal python3-venv librrd8t64 \
    && rm -rf /var/lib/apt/lists/* /var/cache/apt/* /var/cache/debconf/*-old \
              /var/log/apt/* /var/log/dpkg.log /usr/share/fonts \
    && ! command -v python3

# No missing library, and the module exports the version string cbd checks
# (config::applier::modules::_check_module) before loading it.
RUN for lib in /usr/sbin/cbd /usr/sbin/cbwd /usr/share/centreon/lib/centreon-broker/*.so; do \
      if ldd "$lib" | grep "not found"; then echo "$lib: missing library" >&2; exit 1; fi; \
    done \
 && version=$(/usr/sbin/cbd --version 2>&1 | sed -n 's/.*Centreon Broker \([^ ]*\).*/\1/p' | head -n1) \
 && test -n "$version" \
 && grep -qa "$version" /usr/share/centreon/lib/centreon-broker/70-victoria_metrics.so

# One layer: the base image ships files that the layers above delete
# (Python, the venv, the RRD libraries), which would otherwise still be
# downloaded.
FROM scratch

COPY --from=rootfs / /

# Run time settings of the base image, without the venv in PATH and with cbd
# as the command. The ports of the RRD broker and of the control API are gone.
ENV PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    DEBIAN_FRONTEND=noninteractive \
    APP_SECRET= \
    SALT=
EXPOSE 51003 5669 51001
WORKDIR /var/lib/centreon-broker
USER centreon-broker
HEALTHCHECK --interval=10s --timeout=5s --start-period=30s --retries=3 \
    CMD test -f /tmp/docker.ready || exit 1
ENTRYPOINT ["/usr/local/lib/centreon-broker/container.sh"]
CMD ["/usr/sbin/cbd", "/etc/centreon-broker/central-broker.json"]
