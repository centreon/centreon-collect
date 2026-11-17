# syntax=docker/dockerfile:1
# Images of the integration bench (see compose.yaml), with gorgone-agent
# inside the container it drives:
# - poller: the Centreon poller test image, gorgone-agent in place of the
#   Perl gorgoned;
# - engine: the engine image of this branch (engine/README.md) with its
#   agent, which registers the poller and deploys it when asked.
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
