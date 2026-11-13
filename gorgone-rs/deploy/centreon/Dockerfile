# syntax=docker/dockerfile:1
# The Centreon poller test image with gorgone-agent in place of the Perl
# gorgoned, for the integration bench (see compose.yaml).
# Build context: gorgone-rs/.

ARG POLLER_IMAGE=docker.centreon.com/centreon/centreon-poller-alma9:develop

FROM rust:1.97-alpine AS build
RUN apk add --no-cache musl-dev
WORKDIR /src
COPY Cargo.toml Cargo.lock rust-toolchain.toml ./
COPY crates crates
RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/src/target \
    cargo build --release --locked -p gorgone-agent \
    && mkdir /out \
    && cp target/release/gorgone-agent /out/

FROM ${POLLER_IMAGE} AS poller
COPY --from=build /out/gorgone-agent /usr/bin/
COPY --chmod=755 deploy/centreon/gorgone-rs-lib.sh /usr/share/centreon/gorgone-rs-lib.sh
COPY --chmod=755 deploy/centreon/poller/60-register_central.sh /usr/share/centreon/container.d/60-register_central.sh
RUN mv /usr/bin/gorgoned /usr/bin/gorgoned.perl-disabled
