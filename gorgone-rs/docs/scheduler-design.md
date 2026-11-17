# centreon-engine — design note

Status: proof of concept. The program `centreon-engine` replaces, on a
poller, Centreon Engine (`centengine`) and its Broker module (`cbmod`): it
takes the configuration centreon-web generates (a structured JSON document,
or the Engine `.cfg` files), runs the checks and sends the results straight
to the central Broker (`cbd`) over BBDO. One process per poller, no C++.

- The engine is the library crate `crates/centreon-engine`.
- The program is the second binary of the agent crate
  (`crates/gorgone-agent/src/bin/centreon-engine.rs`, built with the cargo
  feature `scheduler`): the agent with the engine always on, so that the
  poller still runs the jobs of gorgone-server (configuration, external
  commands, service control, `RunCommands`). A bin crate depending on the
  agent library would be the same thing with one more manifest; a binary in
  the engine crate would make the engine depend on the agent (cycle).
- `gorgone-agent` stays as is for the pollers that keep the C++ Engine; built
  with the feature and `[scheduler] enabled`, it runs the same engine.

Every protocol statement below is taken from the C++ code of this repository;
references are `file:line` at the time of writing.

## 1. BBDO as `cbd` expects it from a poller

### Transport: TCP, not gRPC

`cbd` accepts BBDO over TCP (`broker/tcp`, endpoint types `ipv4`/`tcp`) or
gRPC (`broker/grpc`). The scheduler speaks **BBDO over plain TCP**:

- the gRPC transport is a bidirectional stream of `CentreonEvent`
  (`broker/grpc/generate_proto.py:33`, `:301`, `service centreon_bbdo { rpc
exchange(stream CentreonEvent) returns (stream CentreonEvent) }`) whose
  `oneof` is _generated at build time_ from every `bbdo/*.proto`: it is not in
  the repository, its numbers are defined by a script, and on top of it the
  BBDO stream still negotiates and acknowledges (`stream.cc:1457-1490`, the
  `_grpc_serialized_queue` path). gRPC would add a generated contract and
  keep all of the BBDO logic;
- the poller flow centreon-web generates by default, and the central input of
  the bench (`central-broker-master-input`, port 5669), are TCP;
- the TCP framing is 16 bytes of header plus the protobuf payload: a few dozen
  lines on top of `tokio::net::TcpStream` and the `prost` messages the
  workspace already builds. No new dependency.

Compression and TLS are BBDO _extensions_ negotiated in the welcome
(`stream.cc:932-1003`). The engine never announces `COMPRESSION` (a peer
that requires it only logs an error, `stream.cc:983-989`), and announces
`TLS` as the section "TLS to the central broker" below says.

### Framing (`broker/core/bbdo/stream.cc:402-540`, `_read_any` `:1264-1450`)

Each packet is a 16-byte header followed by at most 0xFFFF bytes of payload
(`BBDO_HEADER_SIZE = 16`, `broker/core/bbdo/internal.hh:29`):

| Offset | Size | Field (network byte order)                                     |
| ------ | ---- | -------------------------------------------------------------- |
| 0      | 2    | CRC-16 of bytes 2..16 of the header                            |
| 2      | 2    | payload size; `0xFFFF` means "more parts of this event follow" |
| 4      | 4    | event type: `category << 16 \| element` (`bbdo/events.hh`)     |
| 8      | 4    | source id (0)                                                  |
| 12     | 4    | destination id (0)                                             |

- The checksum is `misc::crc16_ccitt` (`broker/core/src/misc/misc.cc:52-75`):
  nibble table, initial value `0xFFFF`, result complemented. The reader
  skips one byte at a time on a bad CRC (`stream.cc:1300-1320`), so a framing
  bug shows as `peer … is sending corrupted data` in the cbd log.
- An event of 0xFFFF bytes or more is cut in parts of exactly 0xFFFF bytes,
  each with a header announcing 0xFFFF, the last one with the rest
  (`stream.cc:494-530`); the reader glues parts with the same
  type/source/destination (`stream.cc:1370-1440`).
- A BBDO 3 event payload is the protobuf message, serialized as is
  (`broker/core/inc/com/centreon/broker/io/protobuf.hh:167-190`), no wrapper.

Event types used (`bbdo/events.hh`): category `neb` = 1, `bbdo` = 2.

| Event                           | Element | Message (`bbdo/*.proto`)      |
| ------------------------------- | ------- | ----------------------------- |
| `bbdo:welcome`                  | 7       | `Welcome` (`bbdo/bbdo.proto`) |
| `bbdo:pb_ack`                   | 8       | `Ack`                         |
| `bbdo:pb_stop`                  | 9       | `Stop`                        |
| `bbdo:ack` (BBDO < 3.0.1)       | 2       | legacy mapping: one `uint32`  |
| `neb:pb_service`                | 27      | `Service` (`bbdo/neb.proto`)  |
| `neb:pb_service_status`         | 29      | `ServiceStatus`               |
| `neb:pb_host`                   | 30      | `Host`                        |
| `neb:pb_host_status`            | 32      | `HostStatus`                  |
| `neb:pb_instance_status`        | 42      | `InstanceStatus`              |
| `neb:pb_instance`               | 44      | `Instance`                    |
| `neb:pb_instance_configuration` | 54      | `InstanceConfiguration`       |
| `neb:pb_comment`                | 35      | `Comment`                     |
| `neb:pb_downtime`               | 36      | `Downtime`                    |
| `neb:pb_acknowledgement`        | 45      | `Acknowledgement`             |
| `neb:pb_severity`               | 33      | `Severity`                    |
| `neb:pb_tag`                    | 34      | `Tag`                         |
| `neb:pb_log_entry`              | 41      | `LogEntry`                    |
| `neb:pb_host_group`             | 49      | `HostGroup`                   |
| `neb:pb_host_group_member`      | 50      | `HostGroupMember`             |
| `neb:pb_service_group`          | 51      | `ServiceGroup`                |
| `neb:pb_service_group_member`   | 52      | `ServiceGroupMember`          |
| `neb:pb_host_parent`            | 53      | `HostParent`                  |
| `neb:pb_adaptive_service`       | 28      | `AdaptiveService`             |
| `neb:pb_adaptive_host`          | 31      | `AdaptiveHost`                |

There is no dependency event: `de_pb_host_dependency` and
`de_pb_service_dependency` are "Not used" (`bbdo/events.hh:149-150`), and
`broker.cc` sends none in BBDO 3.

The messages are mirrored in `gorgone-rs/proto/com/centreon/broker/bbdo.proto`
with the field numbers of `bbdo/neb.proto` and `bbdo/bbdo.proto`, reduced to
the messages above (the same practice as the engine API mirror), and
generated by `tools/protogen` into `crates/gorgone-proto`.

### Handshake (`stream::negotiate`, `stream.cc:739-1020`)

The connecting side speaks first (`connector.cc:98`, `negotiate_first`); the
`cbd` input answers (`acceptor.cc:104`, `negotiate_second`):

1. poller → `Welcome{version 3.0.1, extensions "", poller_id, poller_name,
broker_name, peer_type ENGINE, extended_negotiation false}`. Above BBDO
   3.0.0 the welcome is the protobuf `pb_welcome` (`stream.cc:757-783`).
2. `cbd` checks the major version (a different major closes the connection,
   `stream.cc:870-880`), downgrades minor/patch to ours if lower, answers
   with its own `Welcome` (`stream.cc:886-915`) and registers the peer
   (`add_peer`, `stream.cc:1012-1016`).
3. With TLS in force (both welcomes list `TLS`), the TLS handshake, then
   everything else inside TLS (below).
4. Nothing else: with `extended_negotiation` false, and no engine
   configuration directory, Engine itself sends no `pb_engine_configuration`
   (`_negotiate_engine_conf`, `stream.cc:1539-1613`, legacy branch
   `:1609-1612`).

The "newer mechanism" — Broker reading the poller configuration itself —
exists (`pb_engine_configuration` answered at `stream.cc:1132-1170`,
`check_poller_configuration` `:1689`; `unified_sql::_process_pb_instance_configuration`
copies the php cache directory and parses `centengine.cfg` with
`common/engine_conf`, `broker/unified_sql/src/stream_sql.cc:1746-1830`), but
only when the central broker has `cache_config_directory`
(`broker/core/src/config/parser.cc:281-288`, `stream_sql.cc:1762-1763`),
which centreon-web does not generate. The scheduler therefore sends the full
configuration as Engine does by default, which works with every central.

### TLS to the central broker

`src/tls.rs`, `src/client.rs`; Broker's side: `broker/tls/` (GnuTLS),
`stream::negotiate` (`broker/core/bbdo/stream.cc:713-1010`).

- **Settings**: the `tls` (`yes` mandatory, `auto` offered, `no`),
  `private_key`, `public_cert` (`certificate` for a `bbdo_client`),
  `ca_certificate` and `tls_hostname` keys of the `cbmod` output
  (`broker/tls/src/factory.cc:42-118`), from the `cbmod` file or
  `broker.tls` of the document (version 4). Paths on the poller.
- **Negotiation** (`stream.cc:947-982`): the engine lists `TLS` in its
  welcome when it can do it; TLS is in force when the welcome of `cbd` lists
  it too. With `tls = yes` and a `cbd` that does not list it, the engine
  refuses the connection, as Broker does; `cbd` refuses an engine that does
  not list it when its input says `yes`.
- **Handshake**: on the same TCP connection, right after the welcomes
  (`set_substream`, `stream.cc:962-965`); `cbd` is the TLS server, the
  engine the client (`tls/src/connector.cc`). The engine checks that `cbd`
  sent nothing after its welcome. Every BBDO frame then goes inside TLS,
  same framing; no second negotiation.
- **Certificates only.** Broker uses certificates when both `public_cert`
  and `private_key` are set, else anonymous Diffie-Hellman (`+ANON-DH`,
  `params.cc:58-91`, `internal.cc:36-46`). rustls (the TLS stack of the
  workspace: gorgone-server and tonic's `tls-ring`) has no anonymous suite
  and the `openssl` crate would link the system library into the static
  binary: without a certificate and a key the engine does not offer the
  extension (`auto`, clear text) or refuses to start the connection
  (`yes`, with that reason). centreon-web's defaults (`tls` `no` on the
  module output, `auto` on the central input, no certificate) give clear
  text, as with Engine.
- **Verification** as `params.cc:226-270`: with `ca_certificate`, the
  certificate of `cbd` is checked against it, and its name against
  `tls_hostname` only when set (`verify_peers3`; `verify_peers2` checks the
  chain only); without a CA, nothing is checked, as Broker. The engine
  presents its certificate: `cbd` asks for one when it has a CA
  (`GNUTLS_CERT_REQUEST`) and checks it the same way.
- **Tests**: `tests/tls.rs` (a fake central broker with rustls, client
  certificate required); `tests/real-cbd/run.sh` with `TLS=1` makes the
  `cbd` input `tls: yes` with certificates of a CA made with `openssl`, and
  checks that `cbd` negotiates TLS, completes the handshake and stores the
  events. The integration bench keeps its plain input.
- No new crate: `rustls` and `tokio-rustls` (ring) were in `Cargo.lock`;
  `rcgen` for the test certificates too.

### Acknowledgements and retention

- The `cbd` input counts every non-`bbdo` event it reads and sends `pb_ack
{acknowledged_events: n}` every 1000 events (`ack_limit`,
  `factory.cc:110`) or after 5 s with something pending (`stream::read`,
  `stream.cc:1215-1262`; `send_event_acknowledgement` `:1663-1687`; the
  legacy `ack` below BBDO 3.0.1). The sender must read the socket: acks are
  small, but an unread socket ends up full.
- The ack means "read by `cbd`", which keeps the event in its own muxer
  queues from there. The scheduler keeps every written event until it is
  acknowledged; on a lost connection the unacknowledged ones are sent again
  first (at-least-once, as `cbmod` retention does). While disconnected, new
  events queue in memory, bounded (`max_pending_events`, default 100 000):
  above it the **oldest** are dropped and counted (a metric point lost rather
  than a scheduler blocked; the next status refreshes the state anyway).
- Stop: `pb_stop{poller_id}` then wait for the ack (`stream.cc:660-711`);
  `cbd` answers the ack and publishes a `local::pb_stop` that makes
  `unified_sql` mark the poller as stopped (`stream.cc:1112-1126`,
  `broker/unified_sql/src/stream.cc:870-900`). Before it, as Engine at the end
  of its loop, `pb_instance{running false, end_time}`
  (`engine/src/broker.cc:4204-4212`).

## 2. What `unified_sql` needs, and in which order

Engine at start (`broker_program_state`, `engine/src/broker.cc:4159-4200`):
`pb_instance{running true, start_time, pid, name, instance_id, engine,
version}`, then `send_initial_configuration` (`:4135-4153`): severities,
tags, hosts, services, custom variables, downtimes, parents, groups, and last
`pb_instance_configuration{loaded true, poller_id}` (`:4113-4132`). Then
statuses as checks complete, and `pb_instance_status` on each status update
(`broker_program_status`, `:4280-4330`).

On the central (`broker/unified_sql/src/stream_sql.cc`):

- `pb_instance` (`:3216-3275`): `_clean_tables(instance_id)` disables every
  host, service and resource of the poller (`:73-160`), then upserts
  `instances` (`running`, `start_time`, `name`…). Everything the poller does
  not send again stays disabled.
- `pb_host` (`:2410-2470`): ignored without `host_id` and a non-empty `alias`
  (`:2429`); upserts `hosts`, fills the host→instance cache and `resources`
  (`_process_pb_host_in_resources`).
- `pb_service` (`:4058-4115`): ignored when the host is unknown
  (`_host_instance_known`); upserts `services`, creates the `index_data` row
  and its cache (`_check_and_update_index_cache`, `:4384-4440`) — without it
  no metric is ever stored (`stream_storage.cc:104-111`) — and `resources`.
- `pb_service_status` (`:4570-4640`): applied to `services`/`resources` when
  passive, or when `next_check >= now - 300`, or `next_check == 0`. The
  storage part (`stream_storage.cc:69-160`) parses the `perfdata` field
  (`common::perfdata::parse_perfdata`), creates `metrics` and publishes
  `storage::pb_metric`/`pb_status` that the `victoria_metrics` output
  forwards. A status whose `(last_check, state, state_type)` equals or
  precedes the last one received for the service is ignored for storage
  (`stream_storage.cc:75-95`).
- `pb_host_status` (`:2719-…`): same freshness rule (`:2742-2746`).
- `pb_instance_status` (`:3328-…`): `last_alive`; Engine sends it every
  `status_update_interval` (default 60 s, `common/engine_conf/state_helper.cc:322`).

Order kept by the engine on every (re)connection:

```
pb_instance(running) → pb_severity × S → pb_tag × T → pb_host × N
→ pb_service × M → pb_downtime × D → pb_comment × C → pb_host_parent × P
→ (pb_host_group, pb_host_group_member × m) × G
→ (pb_service_group, pb_service_group_member × m) × G'
→ pb_instance_configuration → pb_host_status × N → pb_service_status × M
→ pb_instance_status → pb_log_entry (made before the first connection)
```

then statuses as checks complete, log entries, and `pb_instance_status`
every `status_update_interval`. Custom variables are not sent;
`resources` take their severity and tags from `pb_host`/`pb_service`
(`severity_id`, `tags`).

Downtimes and comments are in every dump with their current state, where
Engine sends its downtimes as added (`send_downtimes_list`,
`engine/src/broker.cc:3895-3924`) and its comments not at all: Engine sends
the dump once per process, `cbmod` keeps its events across a lost
connection, while this engine sends the dump on every connection. The
`pb_instance` of each dump makes the central cancel the open downtimes and
delete the non-persistent comments of the poller (`_clean_tables`,
`broker/unified_sql/src/stream_sql.cc:162-185`); the upserts that follow
(`broker/unified_sql/src/stream.cc:1230-1260`, keys entry time, poller and
internal id) put them back as they are.

`pb_instance` carries `is_encryption_ready` when the engine has the key of
the `encrypt::` values (`engine/src/broker.cc:4176`): centreon-web
encrypts the password macros of a poller only once it reported it
(`DbWriteMonitoringServerRepository::updateAllEncryptionReadyFromRealtime`,
run by each generation).

### Reload: only what changed

Engine does not resend its configuration on a reload
(`applier::state::_processing`, `engine/src/configuration/applier/state.cc:1226-1625`):
each object type is diffed (`pb_difference`) and each applier tells the
broker about its own changes:

| Change          | Engine                                                                                                                                                                         | Event                                                                                                                |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------- |
| host added      | `applier::host::add_object`, `NEBTYPE_HOST_ADD` (`host.cc:134`)                                                                                                                | `pb_host`, `enabled` true                                                                                            |
| host changed    | `applier::host::modify_object`, `NEBTYPE_HOST_UPDATE` (`host.cc:387`)                                                                                                          | `pb_host`, `enabled` true                                                                                            |
| host removed    | `applier::host::remove_object`: `NEBTYPE_SERVICE_DELETE` for each of its services, then `NEBTYPE_HOST_DELETE` (`host.cc:428-436`)                                              | `pb_service`, then `pb_host`, `enabled` false (`broker.cc:574`, `:1059`)                                             |
| service added   | `NEBTYPE_SERVICE_ADD` (`service.cc:152`)                                                                                                                                       | `pb_service`, `enabled` true                                                                                         |
| service changed | `NEBTYPE_SERVICE_UPDATE` (`service.cc:407`)                                                                                                                                    | `pb_service`, `enabled` true                                                                                         |
| service removed | `NEBTYPE_SERVICE_DELETE` (`service.cc:464-466`)                                                                                                                                | `pb_service`, `enabled` false                                                                                        |
| severity, tag   | `applier::severity`, `applier::tag`: modified, removed, added (`severity.cc:57-125`, `tag.cc:64-120`)                                                                          | `pb_severity`/`pb_tag`, `MODIFY`, `DELETE`, `ADD`                                                                    |
| host group      | `applier::hostgroup` (`hostgroup.cc:54`, `:96-114`, `:135`); members added at `hostgroup::resolve` (`engine/src/hostgroup.cc:177-183`)                                         | changed members: each old one `enabled` false, the group (`enabled` when it has members), each new one at resolution |
| service group   | `applier::servicegroup` (`servicegroup.cc:88`, `:150-191`), `servicegroup::resolve`                                                                                            | same as host groups                                                                                                  |
| parents         | `applier::host::modify_object` (`host.cc:344-368`): old ones deleted; `host::resolve` adds every parent of every host (`engine/src/host.cc:4007`, `add_child_host` `:335-344`) | `pb_host_parent`, `enabled` false then true                                                                          |
| end of reload   | `cbm->reload()` (`state.cc:1617`), `cbmod::reload` (`broker/neb/src/cbmod.cc:376-391`)                                                                                         | `pb_instance_configuration{loaded}`                                                                                  |

`broker_program_state` sends `pb_instance` only at the start and the end of
the event loop (`broker.cc:4160-4212`): never on a reload. `unified_sql`
applies an `enabled` false host or service to `hosts`/`services` and
`resources` (`stream_sql.cc:2436-2476`, `:4096-4127`) and leaves the other
objects of the poller alone, where a `pb_instance` would disable all of them
first (`_clean_tables`, `:73-160`).

The engine does the same: a new configuration (document or files) is
diffed against the one in force by object key (`host_id`, `(host_id,
service_id)`, group name, `(id, type)` of severities and tags); objects
whose resolved configuration differs are "changed". On the same
connection the central gets, in the order of `applier::state::_processing`
(`state.cc:1477-1530`): severities and tags; the group memberships then
`pb_service` with `enabled` false of the removed services; the
memberships and parents then `pb_host` with `enabled` false of the removed
hosts (`host.cc:410-436`, `service.cc:455-466`); the old parents of the
hosts whose parents changed; `pb_host` of the added and changed hosts
(with their current state, so a kept object keeps its state); host
groups; `pb_service` of the added and changed services; service groups;
the resolution (every parent, the members of the new and changed groups);
then `pb_instance_configuration`; no `pb_instance`, no status. Kept
objects keep their state and their plan. A
(re)connection still sends the full dump above, and a change of the poller
identity or of the central address reconnects, hence a full dump. Commands
and resource macros have no event: they only change the next command lines.
The summary (`… N added, N changed, N removed`) is the first line of the
report the job returns.

## 3. Configuration input

Two inputs give the same model: the structured document (below, no file)
and the Engine files. With `[scheduler] input = "files"`, the engine reads
the files gorgone delivers for the `engine` target, the same files Engine
reads:

- **Main file** `centengine.cfg` (`common/engine_conf/parser.cc:139-205`):
  `key=value` lines; read: `cfg_file`, `cfg_dir`, `resource_file`,
  `interval_length` (default 60), `service_check_timeout` (60),
  `host_check_timeout` (30), `max_service_check_spread` and
  `max_host_check_spread` (5 min), `status_update_interval` (60),
  `broker_module_cfg_file`, `use_timezone`, `retention_update_interval`
  (60 min), `use_retained_scheduling_info` (0), and a
  `broker_module=…cbmod.so <file>` line as Engine's compatibility hook does
  (`:165-205`). Other keys are ignored.
  Relative paths are resolved from the directory of the main file.
- **Object files** (`parser.cc:222-486`): `define <type> {` … `}` blocks,
  `key value` lines (key up to the first blank, value trimmed), comment lines
  starting with `#` or `;`, `\` line continuation. Types read: `command`,
  `host`, `service`, `timeperiod`, `recurringdowntime`, `hostgroup`,
  `servicegroup`, `severity`, `tag`, `hostdependency`, `servicedependency`,
  `contact`, `contactgroup`, `hostescalation`, `serviceescalation`;
  every other type is skipped. Host keys add `check_period`, `timezone`,
  `parents`, `hostgroups`, `severity` (`severity_id`), `category_tags`,
  `group_tags`, `icon_id`, `acknowledgement_timeout`; service keys
  `check_period`, `timezone` (the zone of its host when absent,
  `common/engine_conf/service_helper.cc:313-344`), `servicegroups` and the
  same severity, tags, icon and timeout. Main file keys add
  `log_service_retries`, `log_host_retries`, `log_external_commands`,
  `log_passive_checks`, `log_level_events`, `log_level_checks`,
  `log_level_external_command`, `soft_state_dependencies`,
  `enable_predictive_host_dependency_checks`,
  `enable_predictive_service_dependency_checks`,
  `cached_host_check_horizon`, with Engine's defaults
  (`common/engine_conf/state_helper.cc:242-346`). Batch 3 adds the global
  notification, event handler, flapping and freshness keys of the document
  version 4 below, and the notification, event handler, flapping and
  freshness keys of hosts and services.
- **Templates** (`parser.cc:280-300`, `:636-656`, `_merge` `:695-…`): an
  object with `name` is a template (also when registered); `register 0`
  objects are templates only. `use a,b` resolves `a` (recursively), merges it,
  then `b`: a field is taken from a template only when the object did not set
  it, so the **first** template that sets a field wins; custom variables are
  merged by name across all templates. An unknown template is an error.
- **Aliases** (`host_helper.cc:38-43`, `service_helper.cc:38-48`): `_HOST_ID`
  → `host_id`, `_SERVICE_ID` → `service_id`, `normal_check_interval`,
  `retry_check_interval`, `active_checks_enabled`/`passive_checks_enabled`,
  `description`. Other `_KEY` lines are custom variables (name upper-cased).
  Defaults applied to registered objects (`set_default_values`):
  `check_interval 5`, `retry_interval 1`, `max_check_attempts 3`, active and
  passive checks enabled, host `alias` = `host_name`.
- **Resource file** (`parser.cc:488-519`): `$USERn$=value` lines.
- **Poller identity and central address**: Engine gets them from the `cbmod`
  JSON (`broker_module_cfg_file`, `engine/src/main.cc:290-295`;
  `broker/core/src/config/parser.cc:215-270`): `centreonBroker.poller_id`
  (replaced by `uid` when present), `poller_name`, `broker_name`,
  `bbdo_version`, and the first `output` of type `ipv4`/`tcp`/`ipv6` with its
  `host` and `port`. centreon-web generates it for the poller
  (`www/class/config-generate/broker.class.php`) and gorgone delivers it as
  the `broker` target. The scheduler reads the same file;
  `[scheduler] broker_address` overrides the address.

### Structured configuration (no file)

`[scheduler] input = "structured"` (the default of `centreon-engine`): the
configuration is one JSON document, never a file on the poller.

- **Format**: JSON, documented by
  [`schemas/engine-configuration.v3.json`](../schemas/engine-configuration.v3.json)
  (versions 1 and 2: [`v1`](../schemas/engine-configuration.v1.json),
  [`v2`](../schemas/engine-configuration.v2.json)),
  read by the serde model of `crates/centreon-engine/src/document.rs`. JSON
  rather than protobuf: PHP writes it with `json_encode`, protobuf would
  need a PHP library and generated classes in centreon-web. Content:
  `version`, `poller {id, name}`, `broker {address, name}` (what the `cbmod`
  file gave), `settings` (the globals above), `resource_macros`
  (`$USERn$`), `commands`, `host_templates`, `hosts`, `service_templates`,
  `services`, `timeperiods` and `recurring_downtimes` (version 2),
  `hostgroups`, `servicegroups`, `severities`, `tags`, `host_dependencies`
  and `service_dependencies` (version 3), `contact_templates`, `contacts`,
  `contactgroups`, `host_escalations` and `service_escalations` (version
  4). Field names are the Engine keys after alias resolution
  (`check_interval`, `active_checks_enabled`…), `host_id`/`service_id` for
  `_HOST_ID`/`_SERVICE_ID`, custom macros in `macros` without the leading
  underscore.
- **Templates are sent unresolved**, with their `use` lists, as in the
  files: each object becomes the raw object of the file reader and goes
  through the same template resolution and the same checks (`cfg::build`).
  The inheritance rules (first template wins, macros merged by name, loops,
  unknown templates) therefore stay Engine's and live in one place, the
  error messages are the same, centreon-web sends what its generator
  already builds, and a template shared by thousands of objects is sent
  once. A test (`files_and_document_give_the_same_model`) builds the same
  objects from files and from a document and compares the models.
- **Unknown fields are refused** (`deny_unknown_fields` on every object),
  the whole document with them: a field the engine does not know is a
  behaviour it would silently not have (a check period, a downtime, a
  dependency…). Refusing keeps the configuration in force and tells the
  operator; centreon-web must not send fields the engine does not take yet.
  `version` is read first: another version is refused for that reason. A
  later field is added to version 1 when an older engine refusing it is
  acceptable; a change of meaning makes version 2.
- **Version 2** (this batch): timeperiods, `check_period`, `timezone`,
  recurring downtimes, `use_timezone`, `retention_update_interval`,
  `use_retained_scheduling_info`, and a change of meaning: a macro value
  `encrypt::…` is decrypted, `raw::` removed, as Engine reads its files
  (version 1 values are literal: the web decrypted them). The change of
  meaning makes it a new version rather than fields added to version 1: an
  engine of version 1 would run the plugins with the ciphertext. A version
  1 document using a version 2 field is refused by name. The engine
  reports the versions it reads in its heartbeat (`centreon-engine 0.1.0
(configuration 1-4)`), and gorgone-server refuses (409) a document newer
  than what the node reads before submitting the job; an engine that
  reports no range reads version 1.
- **Version 3** (batch 2b): host and service groups, severities, tags,
  `parents`, `hostgroups`/`servicegroups` of the objects, `severity_id`,
  `category_tags`, `group_tags`, `icon_id`, `acknowledgement_timeout`, a
  service's own `timezone`, host and service dependencies and the event
  log settings above. No field changes meaning, so by the rule above these
  could have been added to version 2. They make a new version all the same:
  centreon-web sends groups, tags and severities for nearly every poller,
  and an engine reading 1-2 would refuse each of those documents at apply
  time, after gorgone-server stored and submitted it; with version 3,
  gorgone-server refuses it up front (409, the node reports `configuration
1-2`) and the operator sees why. The engine reports `configuration 1-3`;
  gorgone-server accepts versions 1 to 3; a version 1 or 2 document using a
  version 3 field is refused by name.
- **Version 4** (batch 3): contacts and contact templates, contact groups,
  host and service escalations, the notification keys of hosts and
  services (`notifications_enabled`, `notification_interval`,
  `first_notification_delay`, `recovery_notification_delay`,
  `notification_options`, `notification_period`, `contacts`,
  `contact_groups`), event handlers (`event_handler`,
  `event_handler_enabled`), flapping (`flap_detection_enabled`,
  `flap_detection_options`, `low_flap_threshold`, `high_flap_threshold`),
  freshness (`check_freshness`, `freshness_threshold`), `is_volatile`, the
  global settings of all of these (`enable_notifications`,
  `notification_timeout`, `log_notifications`, `log_level_notifications`,
  `log_level_runtime`, `date_format`, `illegal_macro_output_chars`,
  `admin_email`, `admin_pager`, `enable_event_handlers`,
  `global_host_event_handler`, `global_service_event_handler`,
  `event_handler_timeout`, `log_event_handlers`, `enable_flap_detection`,
  the four flap thresholds, `check_service_freshness`,
  `check_host_freshness`, the two freshness intervals,
  `additional_freshness_latency`) and `broker.tls`. Nothing changes
  meaning; a new version for the reason of version 3 (centreon-web sends
  contacts for nearly every poller). The engine reports `configuration
1-4`; gorgone-server accepts versions 1 to 4; a version 1 to 3 document
  using a version 4 field is refused by name; centreon-web sends version 4.
- **Relations in the document**: groups as the generator writes them
  (`hostgroup_id`, `hostgroup_name`, `alias`, `members` as host names;
  `servicegroup_id`, `servicegroup_name`, `alias`, `members` as `[host,
service]`, `servicegroup_members`); severities (`id`, `severity_name`,
  `level`, `icon_id`, `type` `host`/`service`); tags (`id`, `tag_name`,
  `type` `hostcategory`, `servicecategory`, `hostgroup`, `servicegroup`);
  dependencies with the `.cfg` keys of the generator as lists
  (`host_name`, `hostgroup_name`, `dependent_host_name`,
  `dependent_hostgroup_name`, `service_description`, `servicegroup_name`,
  `dependent_service_description`, `dependent_servicegroup_name`) and
  `execution_failure_criteria`, `notification_failure_criteria`,
  `inherits_parent`, `dependency_period`. They become the raw objects of
  the file reader. centreon-web leaves out what refers to the meta-service
  host `_Module_Meta`, absent from the document.
- **Contacts in the document**: as the generator writes them
  (`contact.class.php`, `contactgroup.class.php`, `escalation.class.php`):
  contacts with their keys (`host_notification_commands` and
  `service_notification_commands` as lists of command names), templates
  (`register 0`, with `name`, one `use`) in `contact_templates`, contact
  groups with `members` (contact names), escalations with the lists of the
  generator (one service escalation per host and service, as it writes
  them) and only `contact_groups` (escalations have no contacts, in the
  generator as in Engine's model). The web resolves the inheritance of
  contacts itself (`inheritance_mode`, `contact_additive_inheritance`):
  hosts and services carry their resolved `contacts` and
  `contact_groups`, templates none. The files' `+` (additive lists of
  `contacts` and `contact_groups` over the templates, `fill_string_group`)
  is read by the file input; a document list is a plain list. What refers
  to the meta-service host is left out, as for the dependencies.
- **Timeperiods in the document**: `timeperiod_name`, `alias`, `use`
  (included periods: each timeperiod is the template, under its name, of
  those that use it, as the web writes `name` = `timeperiod_name`), the
  weekdays (`HH:MM-HH:MM,…`), `exceptions` as the web stores them
  (`[{days: "december 25", timerange: "00:00-24:00"}]`, the `.cfg` line is
  `days timerange`) and `exclude`. They become the raw objects of the file
  reader: same parser, same templates. Recurring downtimes: `downtime_id`,
  `downtime_name`, `timezone`, `host_ids`, `service_ids` (`[host, service]`)
  and `periods` in the `.cfg` syntax.
- **Delivery**: `PUT /v1/nodes/{node}/engine-configuration` (gorgone-server)
  takes the document (up to `body_limit_bytes`, 256 MiB by default; the
  1 MiB limit of `POST /v1/nodes/{node}/jobs` does not apply), checks its
  syntax and version, stores it compressed with zstd as a blob (a JSON
  poller configuration compresses about tenfold) and submits
  `ApplyEngineConfiguration{target "engine", digest, size}`;
  `{"apply_engine_configuration": {"digest": …}}` on the jobs route applies
  a stored one again. The job carries only the digest: the agent fetches
  the blob with the `FetchBlob` stream, in chunks of at most 1 MiB, as the
  bundles, so the 4 MiB gRPC message limit never applies; the agent bounds
  it with `max_bundle_bytes` (compressed) and `max_unpacked_bytes`
  (document).
- **Apply**: the agent checks the digest, unpacks the blob and hands the
  document to the engine, which parses and checks it off its task, writes it
  to `<state_dir>/engine-configuration/configuration` (digest on the first
  line, then the document; written aside, synced, renamed), then puts it in
  force as one message to its task (the diff above). An invalid document is
  refused before anything is written: the configuration in force and the
  kept one stay. The job gets the report (`Total Errors:` last) as its
  `CommandResult`. Documents are applied one at a time.
- **Restart**: the engine loads the kept document at start, so the poller
  resumes without a push; `scheduler:reload` puts it in force again.
- **Node state**: the agent reports the digest of the document in force (or
  kept) as applied to the target `engine`, and declares that target and the
  capability `apply_engine_configuration` in its `Hello`: the job is routed
  to it, and `requires_latest_config` (an engine restart waiting for the
  latest `engine` push) works as with a bundle.

Checks done when a configuration is validated (they make `ApplyConfig`
refuse a bundle and `ApplyEngineConfiguration` refuse a document; the
document adds: version, unknown field, poller id and name, broker
address): syntax of the files, unknown template, duplicate template
name, host without `host_name` or `host_id`, service without description,
host, `service_id` or check command, service on an unknown host, unknown
check command, duplicate `host_id` or `(host_id, service_id)`, unreadable or
invalid `cbmod` file, a group, severity or tag without name, id, level or
type, a group member, parent, group of an object, tag or severity that does
not exist (`hostgroup::resolve`, `servicegroup::resolve`, `host::resolve`,
`applier::host::add_object`), a dependency without master or dependent, on
an unknown object, group or `dependency_period`, invalid failure criteria,
an invalid `log_level_*`, a contact without name, a contact, contact group
or notification period of an object, a notification command or period of a
contact, a contact group or period of an escalation, an event handler that
does not exist (`applier::contact::resolve_object`, `host::resolve`,
`service::resolve`, `escalation::resolve`), an escalation without object,
invalid notification, escalation or flap detection options. The report ends with `Total Warnings:` / `Total
Errors:` like `centengine -v`, which the agent already reads.

### Macros and command line

- `check_command name!arg1!arg2`: arguments split on `!`, `\` escapes the
  next character, each argument macro-expanded first (`engine/src/utils.cc:150-190`),
  then the command line (`engine/src/commands/command.cc:151`).
- Expansion (`engine/src/macros/process.cc:34-170`): `$$` gives `$`; a
  known macro is replaced; an unknown one becomes empty at the first level
  and is kept as is inside a custom macro value. Supported: `$HOSTNAME$`,
  `$HOSTADDRESS$`, `$HOSTALIAS$`, `$HOSTSTATE$`, `$HOSTSTATEID$`,
  `$HOSTSTATETYPE$`, `$HOSTATTEMPT$`, `$SERVICEDESC$`, `$SERVICESTATE$`,
  `$SERVICESTATEID$`, `$SERVICESTATETYPE$`, `$SERVICEATTEMPT$`, `$ARGn$`,
  `$USERn$`, `$_HOSTxxx$`, `$_SERVICExxx$` (values expanded once more).
  Notification and event handler commands also have the macros of section
  "Notifications" below. `{{$…$}}` tags are not supported.
- **Secrets** (`engine/src/macros/grab_value.cc:1105-1119`): with a valid
  key file, a macro value starting with `encrypt::` is decrypted when a
  command uses it, `raw::` is removed; a value that does not decrypt stays
  as written and the error names the macro only. Without key (or with a
  version 1 document), values are literal. A decrypted value is not
  expanded again. The key is `engine-context.json` (`app_secret`, `salt`),
  read at each configuration apply as Engine does
  (`engine/src/configuration/applier/state.cc:1569-1589`: an absent file
  keeps the keys in force, keys that cannot encrypt then decrypt a probe
  are refused); its path is `scheduler.credentials_file`, by default
  Engine's `/etc/centreon-engine/engine-context.json`, the file of the
  `engine_context` target. Format (`common/crypto/aes256.cc`,
  centreon-web `Security\Encryption`): base64 of IV (16 bytes),
  HMAC-SHA3-512 of the ciphertext (64), AES-256-CBC ciphertext (PKCS#7);
  the HMAC is checked before decrypting. Crates: `aes`, `cbc`, `sha3`
  (RustCrypto, with `cipher`, `inout`, `block-padding`, `keccak`: 7 new
  entries of `Cargo.lock`), `hmac` and `base64` were already built. No
  crate of the workspace had AES-CBC or SHA-3 (`ring` has neither); the
  `openssl` crate would link the system library into a static binary.
  Processed command lines are never logged; the document, the kept
  snapshot and gorgone-server's blob hold the encrypted values only.
- Splitting is Engine's (`common/process/src/process_args.cc:48-150`): no
  shell; blanks separate arguments; `'…'` and `"…"` quote (quotes may start
  inside a word); `\` escapes the next character, `\n \r \t \a \b \v \f`
  give the control character; an unclosed quote is an error.

## 4. Semantics (Nagios 3 / Engine)

- **Intervals**: `check_interval` and `retry_interval` in units of
  `interval_length` seconds. `max_check_attempts` ≥ 1.
- **Results** (`engine/src/commands/raw_v2.cc:395-406`,
  `engine/src/service.cc:1255-1310`): output = stdout, or stderr when stdout
  is empty; timeout → UNKNOWN, `(Process Timeout)`; exit code outside 0..3,
  or killed by a signal → UNKNOWN, `(Return code of N is out of bounds…)`.
  Output parsing is `common::parse_check_output` (`common/src/perfdata.cc:110-194`)
  with escaped new lines: first line = output (before the last `|`) and
  perfdata (after it); following lines = long output, joined by a literal
  `\n`, until a line holds a `|`, after which everything is perfdata.
- **Services**: OK → state type HARD, attempt 1 (recovery is hard); a
  problem after OK starts at attempt 1, SOFT (HARD when `max_check_attempts`
  is 1 or the host is in a hard DOWN state); each further problem result
  increments the attempt; reaching `max_check_attempts` makes it HARD. While
  SOFT the next check is at `retry_interval`, else `check_interval`.
  `last_state_change` changes on any state change, `last_hard_state` and
  `last_hard_state_change` on a hard one, `last_time_<state>` on every result.
- **Hosts**: checked only when they have a check command (else UP, never
  scheduled, as a passive host). Exit 0 or 1 → UP, 2 or 3 → a problem,
  DOWN or UNREACHABLE (parents below). Same SOFT/HARD rules, DOWN and
  UNREACHABLE being one problem for the attempts.
- **Output**: an empty output becomes `(No output returned from plugin)`
  (`(No output returned from host check)` for a host), and `;` becomes `:`
  (`service.cc:1328-1340`, `host.cc:1449-1460`): `;` separates the fields
  of the event log.
- **Active/passive**: `active_checks_enabled 0` objects are not scheduled
  but accept passive results (`PROCESS_*_CHECK_RESULT`) when
  `passive_checks_enabled`; passive results go through the same state machine
  with `check_type PASSIVE`.
- **Timeouts**: `service_check_timeout`, `host_check_timeout`; the plugin's
  process group gets SIGKILL.
- **Out of scope** (not done, nothing emulated; the full list is in
  section 7): OCSP/OCHP,
  connectors (the command line runs as a process), CMA/OpenTelemetry,
  anomaly detection, meta-services, BAM virtual hosts, custom variables
  events, whitelist, environment macros, comments that expire, state
  stalking, volatile services, `host_down_disable_service_checks`, the
  synchronous parent checks of a host with one attempt (below),
  `DEL_*_DOWNTIME_FULL` and `DEL_DOWNTIME_BY_*` (centreon-web sends none),
  propagated downtimes (`SCHEDULE_AND_PROPAGATE_*`), `CURRENT` states
  (this Engine never logs them: `CURRENT_STATES` has no caller).

### Notifications

`src/core/notify.rs` (and `src/cfg/notifications.rs` for the model), a
port of `notifier::notify` (`engine/src/notifier.cc:961-1041`), the
viability checks of each category (`:222-865`), `get_contacts_to_notify`
(`:878-943`), `notification::execute` (`engine/src/notification.cc:61-189`),
`contact::should_be_notified` (`engine/src/contact.cc:815-1020`) and
`notify_contact` (`host.cc:2602-2732`, `service.cc:3207-3335`).

- **Model**: contacts (options, periods and commands per host and service,
  `email`, `pager`, `address1-6`, `timezone`, groups), contact groups
  (members flattened with `contactgroup_members` and the contacts'
  `contactgroups`), escalations expanded per object
  (`applier/hostescalation.cc:79-128`; `first_notification` defaults to
  -2 as the helpers: one without it never applies). Notification flags as
  `notifier::notification_flag`, letters as `message_helper.cc:187-253`
  (`r` recovery, `f` the three flapping flags, `s` downtime). A service
  with neither contacts nor contact groups takes those of its host, its
  notification interval when its own is 0, its notification period when it
  has none (`applier/service.cc:540-580`).
- **Viability** (each reason): forced bypasses everything but the
  recovery; then `enable_notifications` (as `ENABLE_NOTIFICATIONS` left it)
  and the object's `notifications_enabled`. Normal: not in downtime (a
  service also not in its host's), in its notification period (the
  `_for_notif` variant: a period with no valid time is invalid), not
  flapping, volatile passes, HARD, not acknowledged, not OK, the current
  state in its options (a service whose host is not UP and HARD gets
  none), `first_notification_delay` since the hard change, the
  notification failure criteria of its dependencies (batch 2b, same rules
  as execution, quirks included), and no renotification before the
  interval of the last notification (that of its escalation, if any),
  none when it is 0. Recovery: period, downtime, flapping, SOFT and not OK
  wait (`send_later`); not configured, no notification number or no problem
  notified forget the problem. Acknowledgement: not OK. Flapping: the flag
  of its option, one start, a stop or disabled after a start, not in
  downtime. Downtime: its option, the object's own depth 0. Custom: not in
  downtime.
- **Numbers**: a notification other than a recovery increments
  `current_notification_number`, even with nobody to notify; a recovery,
  flapping or custom notification sets it to 0 afterwards; acknowledgement
  and downtime notifications keep it. `last_notification` is set when
  someone was notified, and reset by every change of state (so a change
  passes the interval). The normal notification keeps the contacts it
  reached: the recovery goes to them only (`contact.cc:897-930`).
  Quirks kept: after a flapping notification the number is 0, so the next
  recovery is not sent; `next_notification` and `no_more_notifications`
  are reported but, as in Engine, never computed outside the retention
  (always 0/false here, the retention keeping them).
- **Contacts**: those of the escalations in force (number within
  `first_notification`..`last_notification`, 0 unbounded; state in
  `escalation_options`; in `escalation_period`, process zone), their
  smallest interval becoming the next one; else the object's contacts and
  the members of its contact groups. Each one passes
  `should_be_notified`: its host or service switch, its notification
  period (its zone, `_for_notif`) and its options for the reason. Engine's
  set has no order; the engine sorts the names.
- **Commands**: per contact, each of its host or service notification
  commands, looked up by exact name (no `!` arguments, as Engine), macros
  expanded with `STRIP_ILLEGAL_MACRO_CHARS` (`illegal_macro_output_chars`,
  control characters and DEL removed from the macro values,
  `clean_macro_chars`), split without shell as the checks
  (`process_args`), killed at `notification_timeout`. Deviation: Engine runs
  them synchronously (`my_system_r`, the loop waits); the engine spawns
  them and goes on. The exit code and the output are ignored, as in Engine;
  a timeout gives `Warning: Contact '…' service notification command '…'
timed out after N seconds` (a warning of the notifications logger).
  Centreon's default `host-notify-by-email` / `service-notify-by-email`
  (`/usr/bin/printf … | /bin/mail …`) need a shell, in Engine as here; the
  bench uses a Perl script (`deploy/centreon/centreon-engine/notify-mail.pl`).
- **Macros**: `$NOTIFICATIONTYPE$` (`PROBLEM` for a normal one, else the
  reason: `RECOVERY`, `ACKNOWLEDGEMENT`, `FLAPPINGSTART`…),
  `$NOTIFICATIONNUMBER$`, `$NOTIFICATIONRECIPIENTS$` (the contacts notified
  before this one, as Engine), `$NOTIFICATIONISESCALATED$`,
  `$NOTIFICATIONAUTHOR$` (`NAME`, `ALIAS`), `$NOTIFICATIONCOMMENT$`,
  `$HOST|SERVICENOTIFICATIONNUMBER$`, `…NOTIFICATIONID$`, `$CONTACTNAME$`,
  `$CONTACTALIAS$`, `$CONTACTEMAIL$`, `$CONTACTPAGER$`, `$CONTACTADDRESSn$`,
  `$CONTACTGROUPNAME$`, `$CONTACTGROUPNAMES$`, `$CONTACTTIMEZONE$`,
  `$HOSTOUTPUT$`, `$LONGHOSTOUTPUT$`, `$HOSTPERFDATA$`, `$SERVICEOUTPUT$`,
  `$LONGSERVICEOUTPUT$`, `$SERVICEPERFDATA$`, `$LASTHOSTSTATECHANGE$`,
  `$LASTSERVICESTATECHANGE$`, `$LASTHOSTCHECK$`, `$LASTSERVICECHECK$`,
  `$TOTALHOSTSERVICES$` (`OK`, `WARNING`, `CRITICAL`, `UNKNOWN`),
  `$ADMINEMAIL$`, `$ADMINPAGER$`, and the dates of `get_datetime_string`
  (`engine/src/shared.cc:116-190`) in the zone of the process:
  `$LONGDATETIME$` (`Tue Nov 17 14:03:05 CET 2026`, `Sept` as Engine),
  `$SHORTDATETIME$`, `$DATE$` in `date_format`, `$TIME$`, `$TIMET$`. The
  zone abbreviation is read from the TZif file or the POSIX rule
  (`src/tz.rs`). Not done: the summary macros of all hosts and services
  (`$TOTALHOSTSUP$`…), on-demand macros (`$CONTACTEMAIL:name$`),
  environment macros.
- **When** (`handle_async_check_result`, `host::handle_state`): a service
  HARD problem with its host UP (each result: the interval decides), the
  host of a service not UP, a recovery (twice for a hard one, as Engine:
  the second is dropped); a host on each result (`RECOVERY` when UP);
  `ACKNOWLEDGE_*` with notify; a downtime start (before the depth goes
  up), end (depth back to 0) and cancellation; flapping start, stop and
  disabled (then a recovery).
- **Log lines** (notifications logger, `log_notifications`; Engine's
  default level `error` keeps them out of the event log; centreon-web writes
  the level of its poller logger settings):
  `HOST NOTIFICATION: contact;host;STATE;command;output` and
  `SERVICE NOTIFICATION: contact;host;service;STATE;command;output;` (the
  extra `;` of `service.cc:3286`), `TYPE (STATE)` for another reason,
  `;author;comment` added to an acknowledgement. `cbmod` parses them into
  `pb_log_entry` (contact, command, `status` 0 for another reason: its
  bug, kept).
- **BBDO**: no notification event (Engine sends none); the fields of
  `pb_host_status` / `pb_service_status` (`notification_number`,
  `last_notification`, `next_[host_]notification`,
  `no_more_notifications`), of `pb_host` / `pb_service` (`notify`,
  `default_notify`, intervals, period, `notify_on_*`), and the
  `notifications` switch of `pb_instance_status`.
- **Commands** the web sends: `ENABLE_NOTIFICATIONS`,
  `DISABLE_NOTIFICATIONS` (the next `pb_instance_status` reports it, as
  `update_program_status`), `ENABLE|DISABLE_HOST_NOTIFICATIONS;host`,
  `…_SVC_NOTIFICATIONS;host;service`, `…_HOST_SVC_NOTIFICATIONS;host`
  (its services): nothing when unchanged, else the switch (kept across
  reloads and in the retention, `modified_attributes`) and
  `pb_adaptive_host` / `pb_adaptive_service` with `notify`
  (`broker.cc:489-520`, `:944-1000`). Arguments after the object (the
  legacy pages add a time) are ignored. `SEND_CUSTOM_*_NOTIFICATION` is
  not sent by centreon-web: not done (the custom reason exists in the
  logic). `SET_*_NOTIFICATION_NUMBER`, `DELAY_*_NOTIFICATION`, the contact
  and contact group switches, `*_ALL_NOTIFICATIONS_BEYOND_HOST`: not done.
- **Retention**: the number, times, id and the notifications kept per
  category (with their contacts) are in the retention file; Engine keeps
  the same (`notification_N` of `retention.dat`).

### Event handlers

`src/core/handlers.rs`, `sehandlers.cc`. A service runs its handlers on a
hard change, a soft recovery, each soft retry while its host is UP, a
problem while its host is not UP that changed the hard state
(`service.cc:1646`, `:1665`, `:1882`, `:1901`, `:1998`; the places of
`log_event`), and a volatile HARD problem; a host when its state changes
(`host.cc:2404-2476`). `enable_event_handlers` (as `ENABLE_EVENT_HANDLERS`
left it) and the object's `event_handler_enabled` (which also stops the
global one) allow it; the global handler runs first, then the object's
(`command!args`). Macros as the notifications (without the notification
and contact ones), stripped the same way; no shell; killed at
`event_handler_timeout`, spawned (Engine waits for it). Lines (events
logger, `log_event_handlers`): `SERVICE EVENT HANDLER:
host;service;STATE;TYPE;attempt;handler`, `HOST EVENT HANDLER:
host;STATE;TYPE;attempt;handler`, `GLOBAL HOST EVENT HANDLER:
hostSTATE;…` (the missing `;` of `sehandlers.cc:483`); the global service
line is a debug one in Engine, not sent. A timeout gives `Warning: Service
event handler command '…' timed out after N seconds` with the command
name: Engine prints the processed line, which may hold a decrypted
secret. No BBDO event (`NEBTYPE_EVENTHANDLER_*` are never emitted).
`ENABLE|DISABLE_EVENT_HANDLERS`, `…_HOST_EVENT_HANDLER;host`,
`…_SVC_EVENT_HANDLER;host;service` as the notification switches
(`pb_adaptive_*` with `event_handler_enabled`).

### Flapping

`src/core/flapping.rs`, `src/state.rs` (`FlapState`), `flapping.cc`,
`service.cc:2155-2304`, `host.cc:2071-2234`.

- A history of 21 states (`MAX_STATE_HISTORY_ENTRIES`), the percent of
  state change weighted from 0.75 (oldest) to 1.25 (newest)
  (`service.cc:2225-2252`), computed even when detection is off.
- Services: each result records its state when its option
  (`flap_detection_options`, all by default) holds and its host is UP; the
  host then records its own state at most once per average check interval
  of its services (`host.cc:2110-2135`). Hosts: each check result;
  UNREACHABLE never recorded.
- Thresholds of the object, else `low|high_*_flap_threshold` (20 and 30).
  With `enable_flap_detection` (off by default) and the object's switch:
  between the thresholds nothing changes; below the low one (at or below
  for a host, as Engine) it stops; at or above the high one it starts,
  unless OK/UP and not flapping yet.
- Start: `SERVICE FLAPPING ALERT: host;service;STARTED; Service appears to
have started flapping (x% change >= y% threshold)` (`>` for a host) — a
  warning of the runtime logger, out of the event log with Engine's default
  `log_level_runtime` —, a non-persistent comment of `(Centreon Engine
Process)` (`pb_comment`, entry type FLAPPING), FLAPPINGSTART. Stop: the
  `STOPPED` line (events logger), the comment deleted, FLAPPINGSTOP then
  RECOVERY. Normal notifications are suppressed while flapping, recoveries
  wait.
- `ENABLE|DISABLE_FLAP_DETECTION`, `…_HOST_FLAP_DETECTION`,
  `…_SVC_FLAP_DETECTION`: enabling computes again without recording,
  disabling ends a flapping (FLAPPINGDISABLED then RECOVERY, the host
  `DISABLED` line; the service one is a debug line in Engine), as
  `handle_flap_detection_disabled`. Statuses carry `flapping` and
  `percent_state_change`; the history is in the retention.

### Freshness

`src/core/handlers.rs`, `service::check_result_freshness`,
`is_result_fresh` (`service.cc:3416-3810`, `host.cc:2870-3819`).

- `EVENT_SFRESHNESS_CHECK` / `EVENT_HFRESHNESS_CHECK` every
  `service|host_freshness_check_interval` seconds (60), created when a
  configuration with `check_service_freshness` (default on) or
  `check_host_freshness` (off) is put in force (`applier/scheduler.cc:342-404`).
- An object with `check_freshness`, not running, with active or passive
  checks, not being freshened, in its check period (and for a service a
  check interval or a threshold) is stale when its expiration is past:
  its threshold (`freshness_threshold`, else one check interval — the
  retry interval while SOFT and not OK — plus its latency plus
  `additional_freshness_latency`, 15 s), from the start of the engine while
  never checked, plus `max_*_check_spread` for an active object checked
  before the start, else from its last check plus its execution time.
- Stale: `Warning: The results of service '…' on host '…' are stale by
…d …h …m …s (threshold=…).  I'm forcing an immediate check of the
service.` (runtime warning), then a forced active check now, which runs
  the check command even when active checks are disabled
  (`CHECK_OPTION_FORCE_EXECUTION`). Not again until a result comes.
  Not done: Engine drops the result of the forced check if a passive
  result made the object fresh meanwhile.

### Groups, severities and tags

`src/cfg/relations.rs`, `src/core/relations.rs`. Host groups take their
`members` and the `hostgroups` of the hosts (`host_helper::expand`,
`common/engine_conf/host_helper.cc:330-345`); service groups their
`members` (host, service pairs), the `servicegroups` of the services and,
transitively, the members of their `servicegroup_members`
(`servicegroup_helper::expand`). Severities and tags are keyed by `(id,
type)` (`severity_helper.cc`, `tag_helper.cc`): a host takes a host
severity, a service a service severity; `category_tags` give host or
service category tags, `group_tags` host or service group tags, as
centreon-web writes them (`host.class.php:588-605`, `service.class.php:
261-264`) — the central fills `resources_tags` and `resources.severity_id`
from `pb_host`/`pb_service`. Events: `pb_severity`, `pb_tag` (`ADD` at the
dump), `pb_host_group`/`pb_service_group` with `enabled`, members, all
skipped for a group without id (`forward_pb_group`,
`engine/src/broker.cc:2131-2215`, `forward_pb_group_member` `:2341-2425`).

### Event log

`src/logs.rs`. Engine writes its event log through spdlog; a
`broker_sink_mt` at INFO (`engine/inc/com/centreon/engine/logging/broker_sink.hh`)
hands each line to `broker_log_data`, and `cbmod` parses it into a
`pb_log_entry` (`forward_pb_log`, `set_pb_log_data`,
`engine/src/broker.cc:3252-3720`): type, host, service, status, SOFT/HARD,
attempt, output, ids from the names. The engine builds the same lines
with Engine's format strings and parses them back with a port of
`set_pb_log_data`, quirks included (an empty output keeps the whole
line). Lines sent, each when its logger logs INFO (`log_level_*`):

- `SERVICE ALERT: host;service;STATE;HARD|SOFT;attempt;output`
  (`service::log_event`, `service.cc:2129-2151`) on a hard change, a soft
  recovery, or a retry while the host is up (`service.cc:1620-1980`);
  SOFT lines only with `log_service_retries`.
- `HOST ALERT: host;STATE;type;attempt;output` (`host::handle_state`,
  `host.cc:2358-2500`): a change of state when HARD (SOFT with
  `log_host_retries`), a SOFT result without change with
  `log_host_retries`.
- `INITIAL HOST STATE` / `INITIAL SERVICE STATE` of each object a
  configuration adds, at start and on each reload (`applier::state`,
  `state.cc:1621-1634`). Engine never logs `CURRENT` states.
- `EXTERNAL COMMAND: NAME;args` (`processing.cc:878-891`) of each
  command handled: the check results with `log_passive_checks` on the
  checks logger, the others with `log_external_commands` on the external
  command logger (`ACKNOWLEDGE_*` lines become the acknowledgement types of
  the log).
- `PASSIVE SERVICE CHECK: host;service;state;output` (`service.cc:1396-1404`):
  at INFO whatever `log_passive_checks` says (it drives the legacy log only).
- `HOST|SERVICE DOWNTIME ALERT: …;STARTED|STOPPED|CANCELLED; …`
  (`host_downtime.cc:187`, `:379`, `:454`, `service_downtime.cc:216`,
  `:420`, `:502`) when the downtime depth leaves or reaches 0.
- `Acknowledgement of host '…' just expired`, `Acknowledgement of service
'…' on host '…' just expired` (`host.cc:2347`, `service.cc:835`).
- The notification, event handler, flapping and freshness lines of the
  sections above.

Lines made before the first connection (the initial states of the first
configuration) are kept, at most 10 000, and sent after the dump, as
`cbmod` keeps what Engine writes before it reaches the central. The
central stores them in `logs`; the event logs page and the timeline API
read them.

### Parents and UNREACHABLE

`parents` of a host (`host::resolve` checks they exist,
`engine/src/host.cc:4007-4020`). A host result that is not UP is DOWN when
the host has no parent or one parent UP, else UNREACHABLE
(`determine_host_reachability`, `host.cc:3628-3680`); a passive result 1 or
2 goes through the same rule. Then, as `process_check_result_3x`
(`host.cc:3154-3380`), on-demand checks: a host recovering checks its
parents and children not UP; a host going down checks its children not
UNREACHABLE, and with more than one attempt its parents UP and, at the
next to last attempt with `enable_predictive_host_dependency_checks`, the
hosts it depends on. A service result checks its host as
`handle_async_check_result` does (`service.cc:1408-1422`, `:1580-1620`,
`:1700-1760`): a host never checked and not planned within 5 minutes, a
host not UP when the service is OK, the host UP when a problem changed
state; and the masters of a service at its next to last attempt
(`enable_predictive_service_dependency_checks`). An on-demand check runs
now unless the object is running, has no check command or active checks,
or was checked within `cached_host_check_horizon` (15 s) when Engine uses
the cached state. Differences: Engine runs the parents of a host with one
attempt synchronously (`run_sync_check_3x`) before deciding; the engine
uses their current state and checks them on demand. An on-demand check
replaces the planned check of the object (one plan per object), where
Engine runs it beside the plan. The services of a host not UP are HARD at
once, as before. Events: `pb_host_parent` at the dump, state 2 in
`pb_host_status` and `pb_host`; macros `$HOSTSTATE$` gives `UNREACHABLE`.

### Dependencies

`hostdependency` and `servicedependency` are expanded to one dependency per
(dependent, master), host groups and service groups resolved
(`hostdependency_helper::expand`, `servicedependency_helper::expand`,
`_expand_services` `:301-344`). Criteria letters as
`servicedependency_helper::hook` (`o,w,c,u,p,n,a`; `a` leaves UNKNOWN out)
and `hostdependency_helper::hook` (`o,d,u,p,n,a`). Execution: a scheduled
check is not run when `authorized_by_dependencies(execution)` fails
(`service.cc:3613-3662`, `host.cc:3706-3755`, from `verify_check_viability`
`service.cc:3177-3188`): the master's state (its last hard state while
SOFT, unless `soft_state_dependencies`) is one of the criteria, or it was
never checked with `p`; `inherits_parent` checks the master's own
dependencies. Quirk kept: a dependency outside its `dependency_period` (in
the zone of the process) authorizes everything. The check is planned one
interval later (the retry interval while SOFT, `service.cc:3130-3134`) at
the next valid time, and a status is sent; `last_check` does not move.
Forced checks ignore it (`CHECK_OPTION_FORCE_EXECUTION`). Notification
criteria suppress notifications by the same rules (section "Notifications"). No
BBDO event (see section 1).

### Acknowledgement expiry

`acknowledgement_timeout` (units of `interval_length`,
`applier/service.cc:102`, `applier/host.cc:88`): an acknowledgement
schedules `EVENT_EXPIRE_*_ACK` at its time plus the timeout
(`schedule_acknowledgement_expiration`, `service.cc:935-945`), also for
the acknowledgements of the retention (`applier/scheduler.cc:905-915`).
When it runs, the acknowledgement ends if its time plus the timeout is
not past yet (`check_for_expired_acknowledgement`, `service.cc:828-848`):
Engine's test, which holds when the timer runs on time. A status follows
(and `cbmod`'s deletion of the acknowledgement), and the expiry line.

### Timeperiods and check periods

- **Syntax** (`common/engine_conf/timeperiod_helper.cc:33-430`): weekday
  lines `monday 09:00-12:00,14:00-18:00`, and the exception lines in the
  same order of attempts as the helper's `sscanf` formats: calendar dates
  (`2026-12-25`, `2026-12-24 - 2026-12-26 / 2`), month dates (`december
25`, `february 1 - march 15`), month days (`day 1 - 15`, `day -1`),
  weekdays of a month (`monday 1 january`, `thursday -1 november`) and of
  every month (`monday 2`, `monday 1 - thursday 3`), each with ranges and an
  optional `/ skip`. `exclude a,b` (`+` adds those of the templates).
  Templates (`common/engine_conf/parser.cc:660-695`, `:870-905`): weekdays
  the object leaves empty, exceptions it does not have, exclusions when it
  has none.
- **Computation**: a port of `common/inc/com/centreon/common/timeperiod_utils.hh`
  and `common/src/timeperiod_utils.cc` (`get_next_valid_time_per_timeperiod_impl`,
  `get_next_invalid_time_per_timeperiod`, the `daterange_*_to_time_t`
  functions, `add_round_days_to_midnight`, `calculate_time_from_*`), with
  the same `-1` sentinels and precedence of exceptions over weekdays. One
  quirk is kept: a timeperiod with no valid time within a year is valid
  all the time (`check_time_against_period` compares the next valid time
  with the time asked, and gets the time asked back), so a period meant to
  exclude now must have a valid time later.
- **Zones**: Engine switches `TZ` and calls `localtime_r`/`mktime`
  (`engine/src/timezone_manager.cc`, `timezone_locker`): the object's
  `timezone` (a service takes its host's), else `use_timezone`, else the
  zone of the process. A multi-threaded program cannot change `TZ`, so
  `src/tz.rs` gives the same `localtime_r` and `mktime` (`tm_isdst = -1`)
  for a zone: TZif files of `/usr/share/zoneinfo` (versions 1 to 4, with
  the POSIX rule of their footer) or a POSIX `TZ` string, an unknown zone
  being UTC as for glibc. `mktime` behaves as glibc on DST changes (checked
  against glibc: a skipped 02:30 becomes 03:30, an ambiguous one the first,
  fields normalised). No crate: the zone reading is 250 lines.
- **Checks** (`service::verify_check_viability`, `engine/src/service.cc:3119-3190`,
  rescheduling `:2490-2560`, the same for hosts): a check due outside its
  `check_period` is not run; it is planned at the next valid time after
  one check interval (300 s when 0), or a week later when there is none,
  and a status with the new `next_check` is sent. A planned check (first
  spread, after a result) is moved to the next valid time
  (`service.cc:2028-2037`). Forced checks ignore the period, as
  `CHECK_OPTION_FORCE_EXECUTION`. A reload refuses an unknown
  `check_period` or excluded period (`timeperiod::resolve`,
  `engine/src/timeperiod.cc:220-260`).

### Downtimes

`engine/src/downtimes/downtime_manager.cc`, `host_downtime.cc`,
`service_downtime.cc`; events as `cbmod` writes them
(`broker/neb/src/cbmod.cc:251-372`): `actual_*_time` and `deletion_time`
-1 (unset) until known.

- **Schedule** (`schedule_downtime` `:505-588`): refused when it ends
  before it starts or is over; times capped at 1/1/2100, duration at 366
  days; a fixed downtime lasts `end - start`; a flexible one needs a
  duration (`cmd_schedule_downtime`); a `triggered_by` must exist. Ids
  never reused (`get_next_downtime_id`). Events: `pb_downtime` added, then
  its comment (`entry_type` downtime, internal, "This host has been
  scheduled for fixed downtime from … to …", `subscribe`), then a timer at
  `start_time` unless triggered.
- **Start** (`handle`, `host_downtime.cc:307-505`): a flexible downtime of
  an object OK/UP waits (pending, expired at `end_time + 1`); otherwise
  `pb_downtime{started, actual_start_time}`, `scheduled_downtime_depth`
  - 1 and a status, a timer at `end_time + 1` (fixed) or `now + duration`
    (flexible), and the downtimes it triggers start. A pending flexible
    downtime starts when its object turns to a problem within its window
    (`check_pending_flex_*_downtime`, `downtime_manager.cc:126-235`, called
    on a host state change, `host.cc:2464-2467`, or a service problem change
    with flexible downtimes pending, `service.cc:1974-1980`).
- **Stop**: `pb_downtime{actual_end_time}`, depth − 1 and a status, the
  downtimes it triggered end, then the downtime and its comment are
  deleted (`pb_comment` and `pb_downtime` with `deletion_time`).
- **Cancel** (`DEL_HOST_DOWNTIME;id`, `DEL_SVC_DOWNTIME;id`,
  `unschedule_downtime` `:61-107`): in effect, `pb_downtime{cancelled,
actual_end_time}` and depth − 1; then deleted, with the downtimes it
  triggered. A downtime never started and over is deleted, cancelled.
- **Commands** (`commands.cc:933-1200`): `SCHEDULE_HOST_DOWNTIME`,
  `SCHEDULE_HOST_SVC_DOWNTIME` (each service of the host),
  `SCHEDULE_SVC_DOWNTIME`, `SCHEDULE_HOSTGROUP_HOST_DOWNTIME` (each member),
  `SCHEDULE_HOSTGROUP_SVC_DOWNTIME` (each service of each member),
  `SCHEDULE_SERVICEGROUP_HOST_DOWNTIME` (each host of the members, once),
  `SCHEDULE_SERVICEGROUP_SVC_DOWNTIME` (each member); an unknown group is
  ignored. The comment is one `;` piece, as Engine reads it; the entry
  time is the timestamp of the command. These, `DEL_*` and the
  acknowledgements and comments below are what centreon-web sends
  (`src/Centreon/Domain/Engine/EngineService.php`; the legacy comments
  page sends `DEL_*_COMMENT`); the `_FULL` and `DEL_DOWNTIME_BY_*`
  variants are sent neither by the web nor by its scripts and stay
  refused.
- A reload that removes an object deletes its downtimes, comments and
  acknowledgement (events sent); Engine would only fail to handle them.

### Recurring downtimes

As `engine/src/downtimes/recurring_downtime.cc` and its header (rule in
`docs/architecture/engine.md`), `src/recurring.rs`: `period` lines of
`recurringdowntime_helper.cc:62-150`; occurrences of yesterday, today and
tomorrow in the zone of the rule, else of each host (`compute_occurrences`,
noon of each day, `mktime` of the bounds, empty windows skipped), scheduled
when they start within 600 s or run; once per (rule, target, start) per
rule object; author `Downtime cycle`, comment `[Downtime cycle #id]`; a
host target also gets one downtime per service; a downtime with the same
target, comment and start (also from retention) is not doubled. Run every
60 s and after each apply. A rule of the configuration loaded at start
skips occurrences that started before the process (no catch-up); a rule
added or changed by a reload takes effect at once with nothing handled;
changing or removing a rule deletes its downtimes not started.

### Acknowledgements and comments

- `ACKNOWLEDGE_HOST_PROBLEM`, `ACKNOWLEDGE_SVC_PROBLEM`
  (`cmd_acknowledge_problem`, `commands.cc:821-892`;
  `acknowledge_*_problem` `:2515-2592`): nothing on an OK/UP object;
  type 2 is sticky, else normal; `pb_acknowledgement`, a status with
  `acknowledgement_type` (the central sets `acknowledged` in `services`,
  `hosts`, `resources`), then a comment (`entry_type` acknowledgement,
  internal, persistent as asked). With notify, the ACKNOWLEDGEMENT
  notification is sent before the status (section "Notifications").
- `REMOVE_*_ACKNOWLEDGEMENT` (`:2594-2617`): type none, a status, the
  non-persistent acknowledgement comments deleted.
- Automatic end (`service.cc:1488-1515`, `host.cc:2438-2450`): a normal
  acknowledgement ends with a change of state, a sticky one with the
  recovery; a service OK has none (`:1574-1576`).
- The `pb_acknowledgement` deletion follows `cbmod`'s list
  (`engine/src/broker.cc:2672-2690`, `:4672-4690`): when a status shows no
  acknowledgement, the open one is closed with a `deletion_time`, unless
  the object recovered or a normal acknowledgement saw its state change
  (the central keeps those rows open, as with Engine).
- `ADD_HOST_COMMENT`, `ADD_SVC_COMMENT` (`cmd_add_comment`,
  `commands.cc:157-227`: persistent as `SimpleAtob`, the comment is the
  rest of the line), `DEL_HOST_COMMENT`, `DEL_SVC_COMMENT` (any comment by
  id), `DEL_ALL_HOST_COMMENTS`, `DEL_ALL_SVC_COMMENTS`; ids are the
  smallest free one (`comment::comment`, `engine/src/comment.cc`).

### Retention

`src/core/retention.rs`. Engine's `retention.dat` (`engine/src/retention/`)
is not reused: its format serves the C++ objects; this engine writes one
JSON document of its own, `retention.json` in `state_dir`, written aside,
synced and renamed. Kept: each object's state (state, state type,
attempt, checked, last check and state change times, last hard state,
output, long output, perfdata, latency, execution time, acknowledgement),
its next check, downtimes, comments and open acknowledgements. Written
every `retention_update_interval` minutes (Engine's 60 by default,
`EVENT_RETENTION_SAVE`) and at a clean stop, before `pb_instance(running
false)`. Read once at start; the first configuration put in force takes
the states of the objects that still exist, before the connection dump,
so the central never sees them PENDING. Retained next checks are used only
with `use_retained_scheduling_info` (Engine's default is 0: checks are
spread again). As Engine with the downtimes of its retention, downtimes not
over are registered again (new comment, start again, depth recomputed);
downtime comments are not restored (made again), a triggered downtime
without its parent is dropped (`xdddefault_validate_downtime_data`).
Objects, downtimes and comments whose object is gone are skipped. An
unreadable file is logged and ignored.

## 5. Scalability

- **One core task** owns every host and service (no lock): a binary heap of
  `(due instant, sequence, object)` drives the schedule; checks are spawned
  as tokio tasks; results come back on a channel; the core applies them,
  reschedules and emits BBDO events. External commands and reloads are
  messages to the same task.
- **Bounded concurrency**: at most `max_concurrent_checks` (default 256)
  plugins at once; due checks beyond that wait in a ready queue, oldest first.
- **Spreading of first checks**: object _i_ of _n_ starts at
  `i × min(interval, max_check_spread) / n`, as Engine's inter-check delay.
  A reload keeps the schedule of unchanged objects.
- **Process spawning** does not block: `tokio::process`, own process group,
  output read up to `max_output_bytes` (default 8 KiB, the Engine default
  output cap is of the same order), the rest discarded.
- **BBDO writes are batched**: the writer drains everything queued into one
  buffer, writes it, and the acknowledgement reader runs in its own task.
  Reconnection with exponential backoff (1 s to 30 s).
- **Memory per object**: a few hundred bytes (names, command line template,
  state, last output and perfdata).
- **Horizontal**: more pollers, as today; a poller is one process.

Measured (README, `examples/load.rs`): 10 000 services every 5 minutes
with `/bin/true`, the scheduler on one thread: 33.3 checks/s for 1.4 % of
one core (about 0.4 ms of CPU per check, process spawning included) and
36 MiB resident; every minute, 166.7 checks/s for 4.2 %.

## 6. The centreon-engine program

`centreon-engine [-c FILE] [--log LEVEL] [--check]`: the configuration file
is `/etc/centreon-engine/centreon-engine.toml` (`CENTREON_ENGINE_CONFIG`),
the log level `CENTREON_ENGINE_LOG`, in the format of the agent. The
environment overrides keys with `GORGONE_AGENT__SECTION__KEY` and, over it,
`CENTREON_ENGINE__SECTION__KEY`: both are read so that a poller moving from
`gorgone-agent` keeps its environment (node id, token), while the new name
wins. `[scheduler] enabled` is forced on and `input` defaults to
`structured`.

```toml
state_dir = "/var/lib/centreon-engine" # required with input = "structured"

[scheduler]
input = "structured"         # or "files"
target = "engine"            # files: target whose path holds the .cfg files;
                             # structured: target the document is reported as
main_file = "centengine.cfg" # files: relative to the target path
# broker_config = "/etc/centreon-broker/poller-module.json" # files: else from centengine.cfg
# broker_address = "broker:5669" # over the document or the cbmod file
max_concurrent_checks = 256
max_pending_events = 100000

[services.engine]
reload = "scheduler:reload"
restart = "scheduler:reload"
```

- At start the engine loads the kept document (structured) or the files of
  the target (files); without one it waits for a deployment. Then it
  connects to the central broker and starts checking.
- **Instance**: `instances.engine` is `Centreon Engine (Rust)`;
  `instances.version` is `27.01.0`, the collect release whose configuration
  and BBDO contract the engine follows, not the crate version: centreon-web
  compares it with Engine versions (`www/class/config-generate/engine.class.php:356-368`
  adds the legacy `cbmod.so` line below 25.05.0, `broker.class.php`
  `getPollerVersion` picks features by version), and `0.1.0` would read as
  a very old Engine.
- **Validation** (files input): a target with `engine_main_file` is checked
  by the engine's own parser (`ConfigChecker::Scheduler`), in place of the
  engine API or `verify_command`: same staging, same `Total Errors:` report,
  same refusal of an invalid bundle.
- **Reload**: `scheduler:reload` (service operation) reads the live files,
  or the kept document, again and applies them in process; a configuration
  that does not parse is refused and the running one is kept. `restart` is
  the same.
- **External commands** (`EngineCommands`): after the agent allowlist,
  handled in process: `SCHEDULE_FORCED_HOST_CHECK`, `SCHEDULE_FORCED_SVC_CHECK`,
  `SCHEDULE_HOST_CHECK`, `SCHEDULE_SVC_CHECK`, `PROCESS_HOST_CHECK_RESULT`,
  `PROCESS_SERVICE_CHECK_RESULT`, `SCHEDULE_HOST_DOWNTIME`,
  `SCHEDULE_HOST_SVC_DOWNTIME`, `SCHEDULE_SVC_DOWNTIME`,
  `SCHEDULE_HOSTGROUP_HOST_DOWNTIME`, `SCHEDULE_HOSTGROUP_SVC_DOWNTIME`,
  `SCHEDULE_SERVICEGROUP_HOST_DOWNTIME`, `SCHEDULE_SERVICEGROUP_SVC_DOWNTIME`,
  `DEL_HOST_DOWNTIME`,
  `DEL_SVC_DOWNTIME`, `ACKNOWLEDGE_HOST_PROBLEM`, `ACKNOWLEDGE_SVC_PROBLEM`,
  `REMOVE_HOST_ACKNOWLEDGEMENT`, `REMOVE_SVC_ACKNOWLEDGEMENT`,
  `ADD_HOST_COMMENT`, `ADD_SVC_COMMENT`, `DEL_HOST_COMMENT`,
  `DEL_SVC_COMMENT`, `DEL_ALL_HOST_COMMENTS`, `DEL_ALL_SVC_COMMENTS`,
  `ENABLE|DISABLE_NOTIFICATIONS`, `…_HOST_NOTIFICATIONS`,
  `…_SVC_NOTIFICATIONS`, `…_HOST_SVC_NOTIFICATIONS`,
  `…_EVENT_HANDLERS`, `…_HOST_EVENT_HANDLER`, `…_SVC_EVENT_HANDLER`,
  `…_FLAP_DETECTION`, `…_HOST_FLAP_DETECTION`, `…_SVC_FLAP_DETECTION`. Any
  other command fails the job with "not supported by the built-in
  scheduler"; lines after it are not run. The lines are not written to
  the engine's own log (they carry comments); they go to the event log as
  Engine's `EXTERNAL COMMAND` lines (above).
- **Key and retention**: `scheduler.credentials_file` (default
  `/etc/centreon-engine/engine-context.json`); the poller declares the
  `engine_context` target and centreon-web pushes the key before each
  document, which requires it (`GorgoneCommandService::sendEngineConfiguration`).
  The retention file is `<state_dir>/retention.json`.
- **Node state**: the heartbeat reports the engine state as
  `reachable = configuration loaded`, version `centreon-engine <version>
(configuration 1-4)`,
  and, with the structured input, the digest of the document in force as
  applied to `target`.
- The crate is a separate library; without the feature the agent binary
  does not contain it, and the `centreon-engine` binary is not built.

## 7. Growing the model (next batches)

Done in batch 2a: timeperiods, check periods and zones, downtimes,
recurring downtimes, acknowledgements, comments, retention, `encrypt::`
macros. Done in batch 2b: groups, severities, tags, the event log, parents
and UNREACHABLE, dependencies (execution), acknowledgement expiry, group
downtimes, a service's own zone (sections 3 and 4). Done in batch 3:
notifications (contacts, contact groups, escalations, notification
commands and macros, the notification criteria of dependencies, the notify
flag of acknowledgements, downtime and flapping notifications), event
handlers, flapping, freshness, the notification, event handler and flap
detection switches, TLS to `cbd` (certificates).

What the C++ Engine (with `cbmod`) does that this engine still does not:

- **Commands**: connectors (Perl and SSH connectors; the command line runs
  as a process), the command whitelist (`/etc/centreon-engine-whitelist`),
  environment macros (`enable_environment_macros`), on-demand and summary
  macros, `{{$…$}}` tags, OCSP/OCHP (`obsess_over_*`, `ocsp_command`,
  `ochp_command`: centreon-web does not even write them; its
  `START|STOP_OBSESSING_*` commands are refused), `PROCESS_FILE`.
- **CMA / OpenTelemetry**: the OpenTelemetry module (OTLP, Telegraf, the
  Centreon Monitoring Agent and its configuration, `-k` CA), so no agent
  host or service.
- **Anomaly detection** services (`type` anomaly detection, thresholds
  from the central), **meta-services** (the `_Module_Meta` host and its
  virtual services: left out of the document by centreon-web) and **BAM**
  virtual hosts and services, which Engine carries for the central.
- **External commands** not handled (refused): checks and passive checks
  switches (`ENABLE|DISABLE_SVC_CHECK`, `…_HOST_CHECK`,
  `…_PASSIVE_*_CHECKS`, `START|STOP_EXECUTING_*`,
  `START|STOP_ACCEPTING_PASSIVE_*`, `…_HOST_SVC_CHECKS`), which the
  legacy pages and widgets send; freshness switches; `CHANGE_*` commands;
  `SEND_CUSTOM_*_NOTIFICATION`, `SET_*_NOTIFICATION_NUMBER`,
  `DELAY_*_NOTIFICATION`, contact and contact group switches,
  `*_ALL_NOTIFICATIONS_BEYOND_HOST`, `*_HOST_AND_CHILD_NOTIFICATIONS`;
  `DEL_*_DOWNTIME_FULL`, `DEL_DOWNTIME_BY_*`, propagated downtimes
  (`SCHEDULE_AND_PROPAGATE_*`); `PROCESS_FILE`. The enginerpc gRPC API.
- **Scheduling details**: the synchronous parent checks of a host with one
  attempt, on-demand checks beside the plan, the inter-check delay and
  interleave methods (`service_inter_check_delay_method`…), the reaper,
  `host_down_disable_service_checks`, state stalking, volatile services
  beyond notifications and event handlers, comments that expire, custom
  variables events, `CURRENT` states, the discard of a freshness result
  made stale meanwhile.
- **Notifications**: the summary macros, `next_notification` /
  `no_more_notifications` (computed by Engine only at retention load),
  the synchronous run (the loop waits in Engine; the engine does not),
  `notification_N` retention in Engine's format, the whitelist.
- **Transport**: anonymous TLS (Broker's `ANON-DH`, centreon-web's
  default when TLS is on without certificates), BBDO compression, the gRPC
  transport, the `bbdo_server` outputs, the extended negotiation (Broker
  reading the poller configuration itself), several outputs (one central
  per poller, failover).
- **Files and operations**: `status.dat`, `retention.dat` in Engine's
  format (the engine keeps its own JSON file), the command file (FIFO),
  `centengine -v` on a running poller (the engine checks its own
  configuration), `centenginestats`, the NEB module API.

- **Document**: a new object type is a new top-level list (`timeperiods`,
  `hostgroups`, `severities`…) with its own `$defs` entry in the schema and
  its serde struct in `document.rs`; a new field of an existing object is a
  new optional property (`check_period`, `parents`…). Engine key names, as
  today. The schema test (`the_schema_file_names_the_same_fields`) keeps the
  schema and the model together.
- **Model**: each object type gets a `Kind` and a resolved `*Conf` in the
  model; templates keep going through `cfg::build`, so both inputs keep
  giving the same model. Cross-object checks (unknown timeperiod, parent
  loop) go to `build`, where the file input gets them too.
- **Diff**: the reload diff is per keyed object (`Changes` in `core.rs`).
  A new type adds its key (timeperiod name, group id, `(id, type)` for
  severities and tags) and its Engine events: add/update/delete as in the
  table of section 2 (`applier/hostgroup.cc`, `severity.cc`, `tag.cc`…
  send the group/severity/tag event with `enabled` false on removal), sent
  before the hosts and services that reference them, as Engine applies
  them. Runtime state that is not configuration (downtimes, comments,
  acknowledgements) is not diffed: it lives in the engine, survives a
  reload, and needs retention (a state file next to the kept document) to
  survive a restart.
