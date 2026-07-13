# Architecture

We are showing how to configure gorgone to manage that architecture:

```text

Central server <------- Distant Poller
```
unlike for the pull module, the communication is entirely done on the HTTP(S) websocket.
In our case, we have the following configuration (you need to adapt it to your configuration).

* Central server:
  * address: 10.30.2.203
* Distant Poller:
  * id: 6 (configured in the Centreon interface as **zmq**. You get it in the Centreon interface)
  * address: 10.30.2.179
  * rsa public key thumbprint: nJSH9nZN2ugQeksHif7Jtv19RQA58yjxfX-Cpnhx09s

# Distant Poller

## Installation

The Distant Poller is already installed with Gorgone.

## Configuration

We configure the file **/etc/centreon-gorgone/config.d/40-gorgoned.yaml**:

```yaml
name:  distant-server
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
      token: "1234"
      address: 10.30.2.203
      ping: 1
```

# Central server

## Installation

The Central server is already installed and Gorgone too.

## Configuration

We configure the file **/etc/centreon-gorgone/config.d/40-gorgoned.yaml**:

```yaml

...
gorgone:
    ...
  modules:
    ...
    - name: proxy
      package: "gorgone::modules::core::proxy::hooks"
      enable: true
      httpserver:
        enable: true
        ssl: true
        ssl_cert_file: /etc/centreon-gorgone/keys/certificate.crt
        ssl_key_file: /etc/centreon-gorgone/keys/private.key
        token: "1234"
        address: "0.0.0.0"
        port: 443      
    - name: register
      package: "gorgone::modules::core::register::hooks"
      enable: true
      config_file: /etc/centreon-gorgone/nodes-register-override.yml
    ...
```

We create the file **/etc/centreon-gorgone/nodes-register-override.yml**:

```yaml
nodes:
  - id: 6
    type: pullwss
    prevail: 1
```
