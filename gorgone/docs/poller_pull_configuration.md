# Pull mode configuration

In pull mode the connection is **initiated by the Poller**, which connects to the Central's ZMQ port. The Central never opens a connection to the Poller.


```text
Central server <------- Distant Poller
```

For an overview of all communication modes and when to use each one, see [communication_modes.md](communication_modes.md).

>The pullwss mode is similar and preferred over this method. This is documented for historic purpose and for users who cannot use pullwss for some reason.

## Example topology

* Central server: `10.30.2.203`
* Distant Poller:
  * id: `6` (Poller id in the Centreon database, either configured in mode "pull" or with the register module active)
  * address: `10.30.2.179`
  * rsa public key thumbprint: `nJSH9nZN2ugQeksHif7Jtv19RQA58yjxfX-Cpnhx09s`

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

    - name: pull
      package: "gorgone::modules::core::pull::hooks"
      enable: true
      target_type: tcp
      target_path: 10.30.2.203:5556
      ping: 1
```

## Central server configuration

File: **/etc/centreon-gorgone/config.d/40-gorgoned.yaml**

```yaml
gorgone:
  gorgonecore:
    external_com_type: tcp
    external_com_path: "*:5556"
    authorized_clients:
      - key: nJSH9nZN2ugQeksHif7Jtv19RQA58yjxfX-Cpnhx09s

  modules:
    - name: proxy
      package: "gorgone::modules::core::proxy::hooks"
      enable: true
    - name: nodes
      package: "gorgone::modules::centreon::nodes::hooks"
      enable: true
```

if you want to use the `register` module you need to enable it in the previous file and add this new one:

File: **/etc/centreon-gorgone/nodes-register-override.yml**

```yaml
nodes:
  - id: 6
    type: pull
    prevail: 1
```

### Why `prevail: 1` is required

The `nodes` module periodically reads the Centreon database and sends node registrations to the `proxy` module. Without `prevail: 1`, the automatic registration from the database could overwrite the pull-mode entry with a push_zmq entry (which would be wrong, as the Central cannot reach the Poller directly).

`prevail: 1` tells the `proxy` module to keep the manual registration and ignore database-sourced updates for that node id.