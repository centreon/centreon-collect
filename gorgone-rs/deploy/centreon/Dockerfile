# syntax=docker/dockerfile:1
# Centreon test images for the integration bench (see compose.yaml):
# - web: centreon-web with the sources of the branch that sends its commands
#   to the Gorgone API (named build context `centreon-src`), and no Gorgone
#   daemon besides a gorgone-agent for the central poller;
# - poller: the stock poller image with gorgone-agent instead of gorgoned.
# Build context: gorgone-rs/.

ARG WEB_IMAGE=docker.centreon.com/centreon/centreon-web-alma9:develop
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

FROM ${WEB_IMAGE} AS web
# Sources of the branch over the installed ones. The installer directory is
# left out: the image removed it once the installation was done.
RUN --mount=type=bind,from=centreon-src,source=centreon,target=/centreon-src \
    tar -C /centreon-src --exclude=./www/install --exclude=./www/front_src \
        --exclude='node_modules' -cf - ./src ./www ./config \
    | tar -C /usr/share/centreon -xf - --no-same-owner --no-overwrite-dir \
    && chown -R centreon:centreon /usr/share/centreon/src /usr/share/centreon/www
COPY --from=centreon-src --chmod=755 .github/docker/centreon-web/alma9/entrypoint/container.d /usr/share/centreon/container.d
COPY --from=build /out/gorgone-agent /usr/bin/
COPY --chmod=755 deploy/centreon/gorgone-rs-lib.sh /usr/share/centreon/gorgone-rs-lib.sh
COPY --chmod=755 deploy/centreon/web/74-gorgone_agent_background.sh /usr/share/centreon/container.d/74-gorgone_agent_background.sh
# The Perl daemon must not come back: anything still calling it fails loudly.
RUN mv /usr/bin/gorgoned /usr/bin/gorgoned.perl-disabled

FROM ${POLLER_IMAGE} AS poller
COPY --from=build /out/gorgone-agent /usr/bin/
COPY --chmod=755 deploy/centreon/gorgone-rs-lib.sh /usr/share/centreon/gorgone-rs-lib.sh
COPY --chmod=755 deploy/centreon/poller/60-register_central.sh /usr/share/centreon/container.d/60-register_central.sh
RUN mv /usr/bin/gorgoned /usr/bin/gorgoned.perl-disabled
