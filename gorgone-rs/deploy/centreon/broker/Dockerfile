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
# entrypoint of this checkout) brings the control API of
# gorgone-rs/docs/containers-contract.md ("Broker control API"): PUT /files,
# bearer token, BROKER_WAIT_FOR_CONFIG.

ARG BASE_IMAGE=docker.centreon.com/centreon/centreon-broker-trixie:develop
FROM ${BASE_IMAGE}

# cbd is unchanged: the module embeds http_tsdb and resolves the Broker core
# (global cache, multiplexing) from the cbd of the base image, built from the
# same broker/, common/, bbdo/ and clib/ sources.
COPY --chown=root:root --chmod=755 70-victoria_metrics.so \
     /usr/share/centreon/lib/centreon-broker/70-victoria_metrics.so

# Same mode and owner as the base image (root-owned, not writable by cbd).
COPY --from=entrypoint --chmod=755 . /usr/local/lib/centreon-broker/

USER root
# No missing library, and the module exports the version string cbd checks
# (config::applier::modules::_check_module) before loading it.
RUN ! ldd /usr/share/centreon/lib/centreon-broker/70-victoria_metrics.so | grep "not found" \
 && version=$(/usr/sbin/cbd --version 2>&1 | sed -n 's/.*Centreon Broker \([^ ]*\).*/\1/p' | head -n1) \
 && test -n "$version" \
 && grep -qa "$version" /usr/share/centreon/lib/centreon-broker/70-victoria_metrics.so
USER centreon-broker
