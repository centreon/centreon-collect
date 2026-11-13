# syntax=docker/dockerfile:1
# Builds static binaries and one minimal image per binary:
#   docker build -f deploy/Dockerfile --target server -t gorgone-server .
#   docker build -f deploy/Dockerfile --target agent  -t gorgone-agent  .

FROM rust:1.97-alpine AS build
RUN apk add --no-cache musl-dev
WORKDIR /src
COPY Cargo.toml Cargo.lock ./
COPY crates crates
RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/src/target \
    cargo build --release --locked -p gorgone-server -p gorgone-agent \
    && mkdir /out \
    && cp target/release/gorgone-server target/release/gorgone-agent /out/

FROM gcr.io/distroless/static-debian12:nonroot AS server
COPY --from=build /out/gorgone-server /usr/bin/gorgone-server
# REST, agent gateway, peer
EXPOSE 8085 5556 5557
ENTRYPOINT ["/usr/bin/gorgone-server"]

FROM gcr.io/distroless/static-debian12:nonroot AS agent
COPY --from=build /out/gorgone-agent /usr/bin/gorgone-agent
ENTRYPOINT ["/usr/bin/gorgone-agent"]
