# Proxy

## Description

This module aims to give the possibility to Gorgone to become distributed.

It is not needed in a Centreon standalone configuration, but must be enabled if there is Poller or Remote servers.

The module includes mechanisms like ping to make sure nodes are alive, synchronisation to store logs in the Central Gorgone database, etc.

A SSH client library make routing to non-gorgoned nodes possible.

## Configuration

| Directive            | Description                                                                                                                            | Default value |
|:---------------------|:---------------------------------------------------------------------------------------------------------------------------------------|:--------------|
| pool                 | Number of children to instantiate to process events                                                                                    | `5`           |
| synchistory_time     | Time in seconds between two log synchronisations                                                                                       | `60`          |
| synchistory_timeout  | Time in seconds before log synchronisation is considered timed out                                                                     | `30`          |
| ping                 | Time in seconds between two node pings                                                                                                 | `60`          |
| pong_discard_timeout | Time in seconds before a ping is considered lost                                                                                       | `300`         |
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

This module uses **register** and **node** modules to get the list of nodes to manage.

This module uses multiple processes: one controls a pool of workers to process events and optionally an httpserver process if pullwss is used.

#### File roles

| File           | Role                                                                                                                     |
|:---------------|:-------------------------------------------------------------------------------------------------------------------------|
| `hooks.pm`     | Runs inside the main Gorgone process. Holds all state (node registry, routing tables, sync timestamps). Routes incoming messages to the right worker. |
| `class.pm`     | Each worker process is an instance of this class. Holds the actual network connections to remote nodes (ZMQ or SSH).     |
| `httpserver.pm`| A dedicated worker process for `wss` and `pullwss` connections. Started only when `httpserver.enable: true`.             |
| `sshclient.pm` | SSH connection helper used by `class.pm` for `push_ssh` nodes.                                                           |

#### Worker pool initialisation

During `init()`, `hooks.pm` forks N worker processes (default 5, set by `pool` in config):

```
hooks.pm::init()
  → fork() × pool_size  →  class.pm::run()  per worker
```

Each worker:
1. Connects to the internal ZMQ bus as a DEALER socket with identity `gorgone-proxy-{pool_id}` (its **control channel**).
2. Immediately sends `PROXYREADY` with its `pool_id` to signal readiness.
3. Starts an EV event loop waiting for messages.

`hooks.pm` marks the worker as `ready` when it receives `PROXYREADY`. It then replays any `PROXYADDNODE` messages for nodes already assigned to that worker (in case the worker restarted).

If a worker dies, `check()` detects it via the `dead_childs` list and forks a replacement with the same `pool_id`.

#### ZMQ channels

Each worker exposes two types of ZMQ DEALER sockets on the internal bus:

| Identity                          | Created by  | Purpose                                                              |
|:----------------------------------|:------------|:---------------------------------------------------------------------|
| `gorgone-proxy-{pool_id}`        | `class.pm`  | **Control channel.** Receives `PROXYADDNODE`, `PING`, `PROXYCLOSECONNECTION`, etc. |
| `gorgone-proxy-channel-{node_id}`| `class.pm`  | **Per-node data channel.** Created by the worker in `action_proxyaddnode()` when a new node is assigned to it. Allows `hooks.pm` to address a specific worker for a specific node without going through the control channel. |
| `gorgone-proxy-httpserver`        | `httpserver.pm` | Dedicated channel for all `wss`/`pullwss` nodes.              |

When a per-node channel is created, the worker also registers an EV I/O watcher on it so new messages trigger `event()` immediately.

#### Node-to-worker assignment (sticky round-robin)

The assignment logic lives in `hooks.pm::routing()`:

```perl
if (defined($nodes_pool->{$target_parent})) {
    $pool_id = $nodes_pool->{$target_parent};  # already assigned → same worker
} else {
    $pool_id = rr_pool();                       # first message → pick next ready worker
    $nodes_pool->{$target_parent} = $pool_id;  # persist the mapping
}
```

`rr_pool()` cycles through pool IDs 1..N and skips any worker that is not `ready`. Once a node is assigned to a worker, **all subsequent messages for that node go to the same worker**. This is required because the worker owns the live TCP/ZMQ/SSH connection; switching workers would mean reconnecting.

The mapping is stored in `$nodes_pool` (node_id → pool_id) and cleared on `UNREGISTERNODES`.

#### Message routing flow

```
Incoming event → hooks.pm::routing()
  1. pathway()            resolve the parent node to reach (handles sub-nodes, static/dynamic routes)
  2. rr_pool()            pick/recall a worker pool_id
  3. choose ZMQ identity  control channel or per-node channel (if channel_ready == 1)
  4. send_internal_message(identity, action, data)
       → worker class.pm::event() → proxy()
           → connect() if no live connection yet
           → clientzmq::send_message()   (push_zmq)
             or sshclient::action()      (push_ssh)
```

For `pull` nodes, `routing()` calls `pull_request()` instead: the message is written directly onto the external ZMQ socket, piggybacking on the persistent connection the poller opened to the central.

For `wss`/`pullwss` nodes, the identity is always `gorgone-proxy-httpserver` regardless of the worker pool.

#### Route resolution: `pathway()`

`pathway()` determines which parent node to use to reach a target. Priority order:

1. **Direct node** — `$register_nodes->{$target}` exists → use it directly.
2. **Static routes** — `$register_subnodes->{$target}{static}` → sorted ascending by `pathscore` (lower score = preferred).
3. **Dynamic routes** — `$register_subnodes->{$target}{dynamic}` → learned from PONG responses, no ordering guarantee.

A candidate is skipped if it is of type `pull`/`wss`/`pullwss` and has never established an inbound connection (no `identity` stored). If all candidates are skipped, `pathway()` falls back to the first candidate regardless, to avoid silently dropping the message.

Static routes are populated from the `nodes` list in the node registry (configured via the `register` module or the Centreon database). Dynamic routes are populated when a PONG response lists the nodes reachable through the responding node.

#### Node types and how they differ in routing

| Type       | Connection direction    | `routing()` sends to         | Connection held by      |
|:-----------|:------------------------|:-----------------------------|:------------------------|
| `push_zmq` | Central → Poller        | `gorgone-proxy-channel-{id}` | `class.pm` worker (clientzmq) |
| `push_ssh` | Central → Poller (SSH)  | `gorgone-proxy-channel-{id}` | `class.pm` worker (sshclient) |
| `pull`     | Poller → Central        | `pull_request()` on external socket | Poller keeps connection open |
| `wss`/`pullwss` | Poller → Central (WS) | `gorgone-proxy-httpserver` | `httpserver.pm`        |

#### Ping / pong

`hooks.pm::ping_send()` is called by `check()` every `ping_interval` seconds (default 60 s). For each registered node it sends a `PING` action through the normal routing path.

- For `push_zmq`/`push_ssh`: the worker sends the ping over the live connection. The remote node replies with `PONG`. On receipt, `hooks.pm` updates `$last_pong->{id}` and calls `register_subnodes()` to refresh dynamic routes from the pong payload.
- For `pull`/`wss`/`pullwss`: the ping is queued for the next time the poller polls. The poller replies with `PONG` over its inbound connection.

If no pong is received within `pong_discard_timeout` seconds (default 300 s), the node is considered stale. After `pong_max_timeout` (default 3) consecutive failures, `hooks.pm` sends `PROXYCLOSECONNECTION` to force the worker to tear down and recreate the connection.

#### Log synchronisation (GETLOG / SETLOGS)

`check()` calls `full_sync_history()` every `synchistory_time` seconds (default 60 s). It sends a `GETLOG` action to every registered node through the normal routing path.

The `GETLOG` payload includes the `ctime` (timestamp of the last log already stored) and `last_id` taken from `$synctime_nodes->{id}` (itself backed by the `gorgone_synchistory` SQLite table). The remote node only returns logs newer than that timestamp.

The remote node replies with one or more `SETLOGS` messages (multiple parts for pullwss, see [pullwss-log-sync.md](./pullwss-log-sync.md)). `hooks.pm::setlogs()` stores each log entry into the local SQLite database and updates `gorgone_synchistory` with the most recent `ctime` seen.

To avoid overlapping sync requests, `$synctime_nodes->{id}{total_msg}` acts as a lock: it is set to `-1` when a `GETLOG` is sent and cleared when all parts are received or the `synchistory_timeout` is exceeded.

## check()

Run by the gorgone-core process regularly (5s).

- start a history synchronization if needed
- delete old history synchronization if older than synchistory_timeout
- launch a ping to all nodes
- delete old pings if older than **pong_discard_timeout**

For the sync history, the process is as follows: [sequence diagram](./pullwss-log-sync.md)

Two things can start a history sync:

* `synchistory_time` configuration, run by `check()`
* an API call to a specific node (in the form [/api/nodes/:nodeid/...](../../api.md)). It is not shown on the above sequence diagram.

