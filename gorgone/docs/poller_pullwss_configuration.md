# Pullwss mode configuration

In pullwss mode the Poller opens a **WebSocket connection** to the Central. No ZMQ port is exposed; the Central listens on a standard HTTP(S) port. This is designed for environments where only outbound HTTPS is allowed from the Poller (e.g. cloud deployments).

```text
Central server <------- Distant Poller
```

For an overview of all communication modes and when to use each one, see [communication_modes.md](communication_modes.md).

The `register` module is an old method of registering Pollers with the Central. It is still supported but the preferred method is to use the `nodes` module, which reads the Centreon database and automatically registers all Pollers inside.

For each gorgone module you can see the dedicated documentation in the [modules](modules) section, documenting every possible parameter.

## Example topology

* Central server: `10.30.2.203`
* Distant Poller:
  * id: `6` (Poller id in the Centreon database)
  * address: `10.30.2.179`

## Distant Poller configuration

File: **/etc/centreon-gorgone/config.d/40-gorgoned.yaml**

```yaml
name: distant-server
description: Configuration for distant server
gorgone:
  gorgonecore:
    id: 6
    privkey: "/var/lib/centreon-gorgone/.keys/rsakey.priv.pem"
    pubkey: "/var/lib/centreon-gorgone/.keys/rsakey.pub.pem"

  modules:
    - name: engine
      package: gorgone::modules::centreon::engine::hooks
      enable: true
      command_file: "/var/lib/centreon-engine/rw/centengine.cmd"

    - name: pullwss
      package: "gorgone::modules::core::pullwss::hooks"
      enable: true
      ssl: true
      port: 443
      token: "your_secret_token"
      address: 10.30.2.203
```

## Central server configuration

The Central requires the `proxy` module (with its httpserver sub-process enabled) and the `nodes` module.

File: **/etc/centreon-gorgone/config.d/40-gorgoned.yaml**

```yaml
gorgone:
  modules:
    - name: proxy
      package: "gorgone::modules::core::proxy::hooks"
      enable: true
      httpserver:
        enable: true
        ssl: true
        ssl_cert_file: /etc/centreon-gorgone/keys/certificate.crt
        ssl_key_file: /etc/centreon-gorgone/keys/private.key
        address: "0.0.0.0"
        port: 443

    - name: nodes
      package: "gorgone::modules::core::nodes::hooks"
      enable: true
```

### Generating a self-signed certificate for testing

```bash
openssl req -x509 -sha256 -nodes -days 365 -newkey rsa:2048 \
  -keyout /etc/centreon-gorgone/keys/private.key \
  -out /etc/centreon-gorgone/keys/certificate.crt
```

Do not use a self-signed certificate in production. Use a certificate signed by a trusted CA and set `https_cert_no_verify: false` on the Poller side.

## Authentication flow

Connection and authentication happen in two steps:

1. **Token check**: the Poller connects to the Central's WebSocket endpoint and sends the shared token in the HTTP `Authorization: Bearer <token>` header.
 - the token is either in the form `Bearer <token_name>:<token_value>` or `Bearer <token>`.
 - If the token contain a name, it's tested against the Centreon API `/administration/tokens/<token_name>` to check that the token exists and is valid.
 - If the token is just a value, it's checked against the gorgone configuration file (httpserver->token).
 - If the token is not found in both cases, the connection is closed.

2. **Node identification**: the first WebSocket message the Poller sends must be a `REGISTERNODES` message containing the Poller's id (or uid). The Central checks that this id exists in its internal node list (populated by the `nodes` module from the Centreon database, or by the `register` module). If the id is unknown, the connection is closed.

Subsequent messages on the same WebSocket connection are processed without re-authentication.

every 5 second check every token from the centreon api used to authenticate are still valid, or disconnect any poller using it.


See the full startup sequence diagram [here](modules/core/pullwss.md).

## uid or id

Both id and uid are supported in gorgonecore->id directive on the poller, see [documentation](configuration.md) "node on id and uid" for more detail.