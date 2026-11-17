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
(`stream.cc:932-1003`): we announce none, so `cbd` applies none unless its
input says `tls = yes` (mandatory), in which case it refuses the connection
(`stream.cc:976-986`). TLS is out of scope for the POC (see risks).

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
3. Nothing else: with `extended_negotiation` false, and no engine
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
pb_instance(running) → pb_host × N → pb_service × M → pb_downtime × D
→ pb_comment × C → pb_instance_configuration → pb_host_status × N
→ pb_service_status × M → pb_instance_status
```

then statuses as checks complete and `pb_instance_status` every
`status_update_interval`. Custom variables, groups, severities, tags and
parents are not sent yet (batch 2b); `resources` are still filled from
`pb_host`/`pb_service`.

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

| Change          | Engine                                                                                                                            | Event                                                                    |
| --------------- | --------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| host added      | `applier::host::add_object`, `NEBTYPE_HOST_ADD` (`host.cc:134`)                                                                   | `pb_host`, `enabled` true                                                |
| host changed    | `applier::host::modify_object`, `NEBTYPE_HOST_UPDATE` (`host.cc:387`)                                                             | `pb_host`, `enabled` true                                                |
| host removed    | `applier::host::remove_object`: `NEBTYPE_SERVICE_DELETE` for each of its services, then `NEBTYPE_HOST_DELETE` (`host.cc:428-436`) | `pb_service`, then `pb_host`, `enabled` false (`broker.cc:574`, `:1059`) |
| service added   | `NEBTYPE_SERVICE_ADD` (`service.cc:152`)                                                                                          | `pb_service`, `enabled` true                                             |
| service changed | `NEBTYPE_SERVICE_UPDATE` (`service.cc:407`)                                                                                       | `pb_service`, `enabled` true                                             |
| service removed | `NEBTYPE_SERVICE_DELETE` (`service.cc:464-466`)                                                                                   | `pb_service`, `enabled` false                                            |
| end of reload   | `cbm->reload()` (`state.cc:1617`), `cbmod::reload` (`broker/neb/src/cbmod.cc:376-391`)                                            | `pb_instance_configuration{loaded}`                                      |

`broker_program_state` sends `pb_instance` only at the start and the end of
the event loop (`broker.cc:4160-4212`): never on a reload. `unified_sql`
applies an `enabled` false host or service to `hosts`/`services` and
`resources` (`stream_sql.cc:2436-2476`, `:4096-4127`) and leaves the other
objects of the poller alone, where a `pb_instance` would disable all of them
first (`_clean_tables`, `:73-160`).

The engine does the same: a new configuration (document or files) is
diffed against the one in force by object key (`host_id`, `(host_id,
service_id)`); objects whose resolved configuration differs are "changed".
On the same connection the central gets, in this order, `pb_service` then
`pb_host` with `enabled` false for removed objects, `pb_host` then
`pb_service` for added and changed ones (with their current state, so a
kept object keeps its state), then `pb_instance_configuration`; no
`pb_instance`, no status. Kept objects keep their state and their plan. A
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
  `host`, `service`, `timeperiod`, `recurringdowntime`; every other type is
  skipped. Host keys add `check_period` and `timezone`, service keys
  `check_period` (a service has the zone of its host,
  `common/engine_conf/service_helper.cc:313-344`).
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
  [`schemas/engine-configuration.v2.json`](../schemas/engine-configuration.v2.json)
  (version 1: [`v1`](../schemas/engine-configuration.v1.json)),
  read by the serde model of `crates/centreon-engine/src/document.rs`. JSON
  rather than protobuf: PHP writes it with `json_encode`, protobuf would
  need a PHP library and generated classes in centreon-web. Content:
  `version`, `poller {id, name}`, `broker {address, name}` (what the `cbmod`
  file gave), `settings` (the globals above), `resource_macros`
  (`$USERn$`), `commands`, `host_templates`, `hosts`, `service_templates`,
  `services`, `timeperiods` and `recurring_downtimes` (version 2). Field names are the Engine keys after alias resolution
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
  engine of version 1 would run the plugins with the ciphertext. The engine
  reads versions 1 and 2; a version 1 document using a version 2 field is
  refused by name. The engine reports `centreon-engine 0.1.0
(configuration 1-2)` in its heartbeat, and gorgone-server refuses (409) a
  document newer than what the node reads before submitting the job; an
  engine that reports no range reads version 1. centreon-web sends
  version 2.
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
invalid `cbmod` file. The report ends with `Total Warnings:` / `Total
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
  `{{$…$}}` tags are not supported.
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
  scheduled, as a passive host). Exit 0 or 1 → UP, 2 or 3 → DOWN (Nagios 3,
  no parents so no UNREACHABLE). Same SOFT/HARD rules with UP/DOWN.
- **Active/passive**: `active_checks_enabled 0` objects are not scheduled
  but accept passive results (`PROCESS_*_CHECK_RESULT`) when
  `passive_checks_enabled`; passive results go through the same state machine
  with `check_type PASSIVE`.
- **Timeouts**: `service_check_timeout`, `host_check_timeout`; the plugin's
  process group gets SIGKILL.
- **Out of scope** (not done, nothing emulated): notifications and
  escalations (and so `notification_period`, the notify flag of
  acknowledgements and the notifications of downtimes), dependencies and
  parents, event handlers, OCSP/OCHP, flapping, freshness, connectors (the
  command line runs as a process), CMA/OpenTelemetry, anomaly detection,
  meta-services, BAM virtual hosts, severities, tags, groups, custom
  variables events, log entries (the event log), whitelist, environment
  macros, acknowledgement expiry (`acknowledgement_timeout`), comments
  that expire, `DEL_*_DOWNTIME_FULL`, `DEL_DOWNTIME_BY_*`, group and
  propagated downtimes (`SCHEDULE_HOSTGROUP_*`, `SCHEDULE_AND_PROPAGATE_*`),
  a service `timezone` of its own.

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
  `SCHEDULE_SVC_DOWNTIME`; the comment is one `;` piece, as Engine reads
  it; the entry time is the timestamp of the command. These, `DEL_*` and
  the acknowledgements and comments below are what centreon-web sends
  (`src/Centreon/Domain/Engine/EngineService.php`; the legacy comments
  page sends `DEL_*_COMMENT`); the `_FULL` variants are not sent by the
  web and stay refused.
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
  internal, persistent as asked). Notify is recorded, not acted on.
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
  `SCHEDULE_HOST_SVC_DOWNTIME`, `SCHEDULE_SVC_DOWNTIME`, `DEL_HOST_DOWNTIME`,
  `DEL_SVC_DOWNTIME`, `ACKNOWLEDGE_HOST_PROBLEM`, `ACKNOWLEDGE_SVC_PROBLEM`,
  `REMOVE_HOST_ACKNOWLEDGEMENT`, `REMOVE_SVC_ACKNOWLEDGEMENT`,
  `ADD_HOST_COMMENT`, `ADD_SVC_COMMENT`, `DEL_HOST_COMMENT`,
  `DEL_SVC_COMMENT`, `DEL_ALL_HOST_COMMENTS`, `DEL_ALL_SVC_COMMENTS`. Any
  other command fails the job with "not supported by the built-in
  scheduler"; lines after it are not run. The lines are not logged (they
  carry comments).
- **Key and retention**: `scheduler.credentials_file` (default
  `/etc/centreon-engine/engine-context.json`); the poller declares the
  `engine_context` target and centreon-web pushes the key before each
  document, which requires it (`GorgoneCommandService::sendEngineConfiguration`).
  The retention file is `<state_dir>/retention.json`.
- **Node state**: the heartbeat reports the engine state as
  `reachable = configuration loaded`, version `centreon-engine <version>
(configuration 1-2)`,
  and, with the structured input, the digest of the document in force as
  applied to `target`.
- The crate is a separate library; without the feature the agent binary
  does not contain it, and the `centreon-engine` binary is not built.

## 7. Growing the model (next batches)

Done in batch 2a: timeperiods, check periods and zones, downtimes,
recurring downtimes, acknowledgements, comments, retention, `encrypt::`
macros (sections 3 and 4). Next (batch 2b): groups, severities, tags, the
event log (`logs`), parents and UNREACHABLE, dependencies; then what
section 4 lists as out of scope.

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
