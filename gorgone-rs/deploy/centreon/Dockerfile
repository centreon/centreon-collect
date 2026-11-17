# syntax=docker/dockerfile:1
# Images of the integration bench (see compose.yaml), with gorgone-agent
# inside the container it drives:
# - poller: the Centreon poller test image, gorgone-agent in place of the
#   Perl gorgoned;
# - engine: the engine image of this branch (engine/README.md) with its
#   agent, which registers the poller and deploys it when asked;
# - rust-poller (runtime) and rust-poller-bench (plus the provisioning
#   script): gorgone-agent with its scheduler, no engine nor broker.
# Build context: gorgone-rs/.

ARG POLLER_IMAGE=docker.centreon.com/centreon/centreon-poller-alma9:develop
ARG ENGINE_IMAGE=centreon-engine-trixie:gorgone-poc
ARG BROKER_IMAGE=centreon-broker-trixie:gorgone-poc

FROM rust:1.97-alpine AS build
RUN apk add --no-cache musl-dev
WORKDIR /src
COPY Cargo.toml Cargo.lock rust-toolchain.toml ./
COPY crates crates
RUN --mount=type=cache,target=/usr/local/cargo/registry,sharing=locked \
    --mount=type=cache,target=/src/target,sharing=locked \
    cargo build --release --locked -p gorgone-agent \
    && mkdir /out \
    && cp target/release/gorgone-agent /out/

FROM ${POLLER_IMAGE} AS poller
COPY --from=build /out/gorgone-agent /usr/bin/
COPY --chmod=755 deploy/centreon/gorgone-rs-lib.sh /usr/share/centreon/gorgone-rs-lib.sh
COPY --chmod=755 deploy/centreon/poller/60-register_central.sh /usr/share/centreon/container.d/60-register_central.sh
RUN mv /usr/bin/gorgoned /usr/bin/gorgoned.perl-disabled

FROM ${ENGINE_IMAGE} AS engine
USER root
# Created first: a COPY --chmod would give its mode to a directory it creates.
# Journal and vmware target of the agent, which runs as the engine user.
RUN install -d -m 755 /usr/local/lib/gorgone \
    && install -d -o centreon-engine -g centreon-engine -m 750 \
      /var/lib/centreon-gorgone /var/lib/centreon-gorgone/agent
COPY --from=build /out/gorgone-agent /usr/bin/
COPY --chmod=644 deploy/centreon/engine/agent.toml deploy/centreon/engine/register.pl \
     /usr/local/lib/gorgone/
COPY --chmod=755 deploy/centreon/engine/50-gorgone-agent_background.sh \
     /usr/local/lib/centreon-engine/container.d/50-gorgone-agent_background.sh
USER centreon-engine

# - broker: the broker image of this branch (broker/README.md) with its
#   agent, the "broker" component of the Central poller; the entrypoint
#   starts it when it finds the binary and its configuration.
FROM ${BROKER_IMAGE} AS broker
# Into existing root-owned directories: no RUN, no shell, one layer each.
COPY --link --chmod=755 --from=build /out/gorgone-agent /usr/bin/gorgone-agent
COPY --link --chmod=644 deploy/centreon/broker/agent.toml \
     /usr/local/lib/centreon-broker/gorgone-agent.toml

# - rust-poller: a poller without centengine nor cbd. gorgone-agent built
#   with its monitoring scheduler (cargo feature `scheduler`) runs the
#   checks and sends the results to the central broker over BBDO
#   (docs/scheduler-design.md), with Debian's basic plugins.
FROM rust:1.97-alpine AS build-scheduler
RUN apk add --no-cache musl-dev
WORKDIR /src
COPY Cargo.toml Cargo.lock rust-toolchain.toml ./
COPY crates crates
RUN --mount=type=cache,target=/usr/local/cargo/registry,sharing=locked \
    --mount=type=cache,target=/src/target,sharing=locked \
    cargo build --release --locked -p gorgone-agent --features scheduler \
    && mkdir /out \
    && cp target/release/gorgone-agent /out/

FROM debian:trixie-slim AS rust-poller
# centreon-web gives new pollers $USER1$=/usr/lib64/nagios/plugins.
RUN apt-get update \
    && apt-get install -y --no-install-recommends monitoring-plugins-basic \
    && rm -rf /var/lib/apt/lists/* /var/cache/apt/* /var/log/apt/* /var/log/dpkg.log \
    && install -d -m 755 /usr/lib64/nagios /etc/centreon-gorgone \
    && ln -s /usr/lib/nagios/plugins /usr/lib64/nagios/plugins \
    && useradd --system --home-dir /var/lib/centreon-gorgone --shell /usr/sbin/nologin \
       centreon-gorgone \
    && install -d -o centreon-gorgone -g centreon-gorgone -m 750 \
       /var/lib/centreon-gorgone /var/lib/centreon-gorgone/agent \
       /var/lib/centreon-gorgone/vmware /etc/centreon-engine /etc/centreon-broker
COPY --from=build-scheduler /out/gorgone-agent /usr/bin/gorgone-agent
COPY --chmod=644 deploy/centreon/rust-poller/agent.toml /etc/centreon-gorgone/agent.toml
USER centreon-gorgone
WORKDIR /var/lib/centreon-gorgone
ENTRYPOINT ["/usr/bin/gorgone-agent", "--config", "/etc/centreon-gorgone/agent.toml"]

# The bench adds what registers the poller in centreon-web and deploys it
# (register.pl, Perl): provisioning only, nothing the monitoring needs.
FROM rust-poller AS rust-poller-bench
USER root
RUN apt-get update \
    && apt-get install -y --no-install-recommends perl \
    && rm -rf /var/lib/apt/lists/* /var/cache/apt/* /var/log/apt/* /var/log/dpkg.log \
    && install -d -m 755 /usr/local/lib/gorgone
# Created first: a COPY --chmod would give its mode to a directory it creates.
COPY --chmod=644 deploy/centreon/engine/register.pl deploy/centreon/rust-poller/objects.clapi \
     /usr/local/lib/gorgone/
COPY --chmod=755 deploy/centreon/rust-poller/entrypoint.sh /usr/local/lib/gorgone/entrypoint.sh
USER centreon-gorgone
ENTRYPOINT ["/usr/local/lib/gorgone/entrypoint.sh"]
