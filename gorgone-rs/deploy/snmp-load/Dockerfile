# syntax=docker/dockerfile:1
# Images of the SNMP load test (README.md). Build context: gorgone-rs/;
# named context `plugins`: a checkout of centreon-plugins (develop).
#
#   docker buildx build -f deploy/snmp-load/Dockerfile --build-context plugins=<checkout> \
#     --target engine -t snmp-load-engine --load .

# The Rust SNMP plugin of centreon-plugins, linked statically against musl
# as its CI does (rust-plugins/README.md).
FROM rust:1.97-alpine AS build-plugin
RUN apk add --no-cache musl-dev
COPY --from=plugins rust-plugins /build
WORKDIR /build
RUN --mount=type=cache,target=/usr/local/cargo/registry,sharing=locked \
    --mount=type=cache,target=/build/target,sharing=locked \
    cargo build --release --target x86_64-unknown-linux-musl \
    && install -D target/x86_64-unknown-linux-musl/release/centreon-plugin-rust-snmp \
       /out/centreon-plugin-rust-snmp

# centreon-engine (scheduler feature) and the BBDO sink.
FROM rust:1.97-alpine AS build-engine
RUN apk add --no-cache musl-dev
WORKDIR /src
COPY Cargo.toml Cargo.lock rust-toolchain.toml ./
COPY crates crates
RUN --mount=type=cache,target=/usr/local/cargo/registry,sharing=locked \
    --mount=type=cache,target=/src/target,sharing=locked \
    cargo build --release --locked -p gorgone-agent --features scheduler --bin centreon-engine \
    && cargo build --release --locked -p centreon-engine --example bbdo_sink \
    && mkdir /out \
    && cp target/release/centreon-engine target/release/examples/bbdo_sink /out/

# The poller: centreon-engine, the Rust plugin with its collections and the
# packaged Perl plugin (centreon-plugin-operatingsystems-linux-snmp from
# apt-plugins-stable, as installed on a production Debian poller).
FROM debian:trixie-slim AS engine
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates gnupg wget \
    && wget -qO- https://apt-key.centreon.com | gpg --dearmor > /etc/apt/trusted.gpg.d/centreon.gpg \
    && echo "deb [arch=amd64] https://packages.centreon.com/apt-plugins-stable/ trixie main" \
       > /etc/apt/sources.list.d/centreon-plugins.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends centreon-plugin-operatingsystems-linux-snmp time \
    && apt-get purge -y gnupg wget && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/* /var/cache/apt/* \
    && useradd --system --home-dir /var/lib/centreon-engine --shell /usr/sbin/nologin centreon-engine \
    && install -d -o centreon-engine -g centreon-engine -m 750 /var/lib/centreon-engine \
       /var/lib/centreon/centplugins \
    && install -d -m 755 /etc/centreon-engine
COPY --from=build-engine /out/centreon-engine /usr/bin/centreon-engine
COPY --from=build-plugin /out/centreon-plugin-rust-snmp /usr/lib/centreon/plugins/
COPY --from=plugins rust-plugins/rs-collections /usr/share/centreon/rs-collections
COPY --chmod=644 deploy/snmp-load/centreon-engine.toml /etc/centreon-engine/centreon-engine.toml
USER centreon-engine
WORKDIR /var/lib/centreon-engine
ENTRYPOINT ["/usr/bin/centreon-engine"]

FROM gcr.io/distroless/static-debian12:nonroot AS sink
COPY --from=build-engine /out/bbdo_sink /usr/bin/bbdo_sink
ENTRYPOINT ["/usr/bin/bbdo_sink", "0.0.0.0:5669", "/data/statuses.csv"]

# SNMP agent: net-snmp snmpd, read-only community "public", HOST-RESOURCES
# and UCD MIBs of the container (the host's CPUs and the overlay mounts).
FROM alpine:3.22 AS snmpd
RUN apk add --no-cache net-snmp \
    && printf 'rocommunity public default\nagentAddress udp:161\n' > /etc/snmp/snmpd.conf
EXPOSE 161/udp
ENTRYPOINT ["/usr/sbin/snmpd", "-f", "-Lo", "-C", "-c", "/etc/snmp/snmpd.conf"]
