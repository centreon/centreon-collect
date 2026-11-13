# syntax=docker/dockerfile:1
# Centreon test images with the Perl gorgoned replaced by the Rust binaries.
# Build context: gorgone-rs/. See compose.yaml in this directory.

ARG WEB_IMAGE=docker.centreon.com/centreon/centreon-web-alma9:develop
ARG POLLER_IMAGE=docker.centreon.com/centreon/centreon-poller-alma9:develop

FROM rust:1.97-alpine AS build
RUN apk add --no-cache musl-dev
WORKDIR /src
COPY Cargo.toml Cargo.lock rust-toolchain.toml ./
COPY crates crates
RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/src/target \
    cargo build --release --locked -p gorgone-server -p gorgone-agent \
    && mkdir /out \
    && cp target/release/gorgone-server target/release/gorgone-agent /out/

FROM ${WEB_IMAGE} AS web
COPY --from=build /out/gorgone-server /out/gorgone-agent /usr/bin/
COPY --chmod=755 deploy/centreon/gorgone-rs-lib.sh /usr/share/centreon/gorgone-rs-lib.sh
COPY --chmod=755 deploy/centreon/web/70-gorgone_background.sh /usr/share/centreon/container.d/70-gorgone_background.sh
# The Perl daemon must not come back: its restart loops become no-ops and the
# program itself is moved away, so anything still calling it fails loudly.
RUN mv /usr/bin/gorgoned /usr/bin/gorgoned.perl-disabled \
    && printf '#!/bin/sh\n# Not needed: the Rust server sees new pollers when their agent connects.\n' \
       | tee /usr/share/centreon/container.d/80-register_pollers_background.sh \
             /usr/share/centreon/container.d/81-register_remote_server_background.sh >/dev/null

FROM ${POLLER_IMAGE} AS poller
COPY --from=build /out/gorgone-agent /usr/bin/
COPY --chmod=755 deploy/centreon/gorgone-rs-lib.sh /usr/share/centreon/gorgone-rs-lib.sh
COPY --chmod=755 deploy/centreon/poller/60-register_central.sh /usr/share/centreon/container.d/60-register_central.sh
RUN mv /usr/bin/gorgoned /usr/bin/gorgoned.perl-disabled
