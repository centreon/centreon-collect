# Proxy

## Description

This module aims to give the possibility to Gorgone to become distributed.

It is not needed in a Centreon standalone configuration, but must be enabled if there is Poller or Remote servers.

The module includes mechanisms like ping to make sure nodes are alive, synchronisation to store logs in the Central Gorgone database, etc.

A SSH client library make routing to non-gorgoned nodes possible.

## Configuration

| Directive            | Description                                                                                                                        | Default value |
|:---------------------|:-----------------------------------------------------------------------------------------------------------------------------------|:--------------|
| pool                 | Number of children to instantiate to process events                                                                                | `5`           |
| synchistory_time     | Time in seconds between two log synchronisations                                                                                   | `60`          |
| synchistory_timeout  | Time in seconds before log synchronisation is considered timed out                                                                 | `30`          |
| ping                 | Time in seconds between two node pings                                                                                             | `60`          |
| pong_discard_timeout | Time in seconds before a ping is considered lost                                                                                   | `300`         |
| buffer_size          | Maximum size of the packet sent from a node to another. This is mainly used by legacycmd to send files from the central to the poller. | `150000`      |


This part of the configuration is only used if some poller must connect with the pullwss module.

| Directive     | Description                                                                                    | Default value |
|:--------------|:-----------------------------------------------------------------------------------------------|:--------------|
| httpserver    | Array containing all the configuration below for a pullwss connection                          | no value.     |
| enable        | Boolean if HTTP server should be enabled                                                       | `false`       |
| ssl           | Should connection be made over TLS/SSL or not                                                  | `false`       |
| ssl_cert_file | Path to a SSL certificate file. required if ssl: true                                          |               |
| ssl_key_file  | Path to a SSL key file associated to the certificate already configured. required if ssl: true |               |
| passphrase    | May be an optional passphrase for the SSL key.                                                 |               |
| token         | Allow to authenticate node. It is required to enable the HTTP server.                          |               |
| address       | Address to listen to. It can be 0.0.0.0 to listen on all IPv4 addresses.                       |               |
| port          | TCP port to listen to.                                                                         |               |


#### Example

```yaml
name: proxy
package: "gorgone::modules::core::proxy::hooks"
enable: false
pool: 5
synchistory_time: 60
synchistory_timeout: 30
ping: 60
pong_discard_timeout: 300
httpserver:  # this is used only if you want to configure pullwss nodes. to make it work you have to add the register module and configure a configuration file for it.
  enable: true
  ssl: true
  ssl_cert_file: /etc/centreon-gorgone/keys/public.pem
  ssl_key_file: /etc/centreon-gorgone/keys/private.pem
  token: secure_token
  address: "0.0.0.0"
```

## Events

| Event           | Description                                                                    |
|:----------------|:-------------------------------------------------------------------------------|
| PROXYREADY      | Internal event to notify the core                                              |
| REMOTECOPY      | Copy files or directories from the server running the daemon to another server |
| SETLOGS         | Internal event to insert logs into the database                                |
| PONG            | Internal event to handle node ping response                                    |
| REGISTERNODES   | Internal event to register nodes                                               |
| UNREGISTERNODES | Internal event to unregister nodes                                             |
| PROXYADDNODE    | Internal event to add nodes for proxying                                       |
| PROXYDELNODE    | Internal event to delete nodes from proxying                                   |
| PROXYADDSUBNODE | Internal event to add nodes of nodes for proxying                              |
| PONGRESET       | Internal event to deal with no pong nodes                                      |

## API

### Copy files or directory to remote server

| Endpoint                   | Method |
|:---------------------------|:-------|
| /api/core/proxy/remotecopy | `POST` |

#### Headers

| Header       | Value            |
|:-------------|:-----------------|
| Accept       | application/json |
| Content-Type | application/json |

#### Body

| Key         | Value                                             |
|:------------|:--------------------------------------------------|
| source      | Path of the source file or directory              |
| destination | Path of the destination file or directory         |
| cache_dir   | Path to the cache directory for archiving purpose |

```json
{
    "source": "<file or directory path>",
    "destination": "<file or directory path>",
    "cache_dir": "<cache directory path>"
}
```

#### Example

```bash
curl --request GET "https://hostname:8443/api/core/proxy/remotecopy" \
  --header "Accept: application/json" \
  --header "Content-Type: application/json" \
  --data " {
    \"source\": \"/var/cache/centreon/config/engine/2/\",
    \"destination\": \"/etc/centreon-engine\",
    \"cache_dir\": \"/var/cache/centreon\"
}"
```

### Developer manual

This module use register and node module to get the list of node to manage.

This module use multiples process : one control, a pool of worker to process event and optionally an httpserver process if pullwss is used.

## check()
Run by gorgone-core process regularly (5s).

- start an history synchronization if needed
- delete old history synchronization if older than synchistory_timeout
- launch a ping to all nodes
- delete old ping if older than pong_discard_timeout

for the sync history, the process is as follows : [sequence diagram](./pullwss-log-sync.mmd)

2 thing can start an history sync, synchistory_time configuration, run by check()
and an api call to a specific node (in the form [/api/nodes/:nodeid/...](../../api.md)). It is not shown on the above sequence diagram.
