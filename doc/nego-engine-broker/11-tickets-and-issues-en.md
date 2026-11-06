# Tickets

[← Previous](10-poller-ha-en.md) · [Index](README-en.md)

<!-- TOC -->
* [First tickets](#first-tickets)
  * [Internal health check in Engine with reporting to Broker](#internal-health-check-in-engine-with-reporting-to-broker)
  * [About diff calculation](#about-diff-calculation)
  * [Study on distribution mechanism](#study-on-distribution-mechanism)
  * [Introduction of Zone and DiffZone messages](#introduction-of-zone-and-diffzone-messages)
* [Multiple tickets in parallel](#multiple-tickets-in-parallel)
  * [Preparing unified_sql](#preparing-unified_sql)
  * [Centralized cache](#centralized-cache)
  * [The conversion block](#the-conversion-block)
* [Evolutions](#evolutions)
  * [Improving the conversion block](#improving-the-conversion-block)
  * [Recovering the health check](#recovering-the-health-check)
* [Moving external command sending to Broker](#moving-external-command-sending-to-broker)
<!-- TOC -->
## First tickets
### Internal health check in Engine with reporting to Broker
Engine retrieves its CPU, MEM occupation, check latency to send a Health message to Broker:
```
message Health {
  uint32 poller_id = 1;
  float cpu = 2; // number between 0 and 1
  float mem = 3; // number between 0 and 1
  float latency = 4;
}
```
This message format gives a goal but should not be taken as final. A study is necessary on useful information for distribution.

A classic system is to weight different indicators to calculate an overall load.
To achieve this, it's good that all parameters are between 0 and 1.
For cpu and memory, the message already contains information at the right scale.
For latency, we must define a threshold beyond which we consider the poller is overloaded. For example, if latency is greater than 10s, we consider the poller is overloaded.

We can now calculate the poller's overall load with the following formula:
C=alpha * cpu + beta * mem + gamma * latency / latency_max with alpha + beta + gamma = 1.

If we find another interesting parameter, we can always add it to the formula and rebalance weights.

A more thorough study could allow us to determine optimal weights for each poller. But empirically, we can start with alpha = 0.4, beta = 0.2 and gamma = 0.4. It seems that cpu and latency have a more important impact than memory hence these parameters.

Once we have this load, we can define a limit load interval. For example, from 60% to 80%; the advantage of having an interval is to be able to work with hysteresis.

* When we want to increase a poller's load, we allow ourselves to do so as long as load is below 60%.
* we consider a poller too loaded when it exceeds 80%.
* When it is too loaded (load > 80%), we try to reduce its load so it goes back below 60%.

The [60%;80%] interval must be configurable.

When a poller is too loaded (so load > 80%), we must remove hosts from it so it goes back below 60%.
The 60% is a target, it's difficult to know exactly the number of hosts to remove to reach it. So the goal is to end up in the interval.

So we have some configuration parameters to define:
* maximum latency (for example 10 seconds)
* CPU, memory and latency load weights (configurable)
* load interval between 60% and 80% (configurable)

### About diff calculation

Broker receives zone configurations, which gives new-zone-1.prot, new-zone-2.prot, ...
These zones are already running and their configuration is in zone-1.prot, zone-2.prot, ...

We then calculate diff-zone-1.prot, diff-zone-2.prot, ...

If zone 1 contains pollers 1, 2 and 3.

The goal would be to distribute modifications on each poller according to its load. And we would use the same machinery as when monitoring pollers.

### Study on distribution mechanism
What load to report to broker and how should Broker adapt distribution to these loads?
It's especially the second part of the question that interests us.

### Introduction of Zone and DiffZone messages
Initial configuration calculation by broker is no longer State/DiffState but Zone / DiffZone.

The Zone is a message very similar to State, it contains the configuration of zone pollers, for now we consider that all pollers are configured the same way. And perhaps this will evolve over time.
The zone carries all hosts/services to be managed by the pollers it contains. However, unlike State, the Zone contains a list of pollers. If global parameters defined at Zone level must be differentiated by poller, we can move them from zone to pollers (at message definition level).

The content of centengine.cfg is a first draft of configuration for the zone. But we must add an array per poller since there are already specific hosts for each.
We can also imagine different log levels per poller.
However, minimum check duration must be shared across the entire zone.

It remains to determine which fields to move per poller.

13 points.

## Multiple tickets in parallel

### Preparing unified_sql
The goal is to write configuration prepared by broker in one go in centreon_storage.

21 points.

### Centralized cache
All streams must access the same cache which comes largely from configuration.

It is composed of two parts:
* configuration
* real-time and other (inheritance from current broker caches)

We must inventory currently used caches. And then we produce a global cache to feed the whole.

We have the global engine configuration which is enriched as configurations are sent on the broker side.
The cache is separate and points to this global configuration.
By doing this, on the Engine side, cbmod also has its cache which this time points to the Engine configuration in globals.cc.
Feeding is templated because done either by Zone or by State.

Possibility to split:
1. we make the new cache. Still quite heavy but reasonable.
2. progressively migrate other caches to it. Migrations can be done in parallel (influxdb, graphite, VictoriaMetrics, Lua, rrd, unified_sql).
3. A ticket already exists to remove sql and storage outputs.

### The conversion block
DiffZone to DiffState, from the zone, we launch one DiffState per poller.

A first step is to consider all pollers identical, and round-robin does the job.

## Evolutions
### Improving the conversion block
Based on the study, distribution on pollers must be improvable.
The block must be configurable using broker's configuration file. One algorithm will surely not be enough to handle all cases.

### Recovering the health check
Taking into account Health by Broker must allow rebalancing poller configurations.

# Potential issues to resolve
* external commands
* problematic passive services
* agent
* retention.dat (on poller side, if it changes we no longer have the info): design settled, see
[Prerequisite 5: removing `retention.dat`](09-ha-preparatory-work-en.md#prerequisite-5-removing-retentiondat).
* hostdependencies / servicedependencies, **execution** dependencies: evaluated locally by
the poller (`host::authorized_by_dependencies`, `service::authorized_by_dependencies`) and
they inhibit the check itself, a synchronous decision on the scheduler hot path — so
co-location remains mandatory. Validation: handled — a cross-poller dependency is a
configuration error caught at `resolve`, thanks to the global view Broker has of the other
pollers' configurations (`foreign_objects`). Beware: a missing master leaves `master_*_ptr`
null and the dependency is then *silently ignored*, hence the validation.
* **notification** dependencies: already evaluated by Broker (`broker_cache`), they could
cross pollers. Still refused today, for lack of a home for a dependency neither poller knows
the far end of — so they still constrain placement, and co-location blocks are computed on
both kinds.
* Hostescalation / Serviceescalation: escalations must follow the notified object (co-location constraint already enforced by the distribution algorithm; handled by the notification service in centralized configuration mode).
* Downtimes and acknowledgements: handled — see [Centralized downtimes and acknowledgements](10-poller-ha-en.md#centralized-downtimes-and-acknowledgements) and [Notifications in HA mode](10-poller-ha-en.md#notifications-in-ha-mode).
* Anomalydetection must be on the same poller as the associated service. And its configuration must follow.
* Very difficult to keep compatibility with old engine behavior
* ping-pong
* Engine configuration check in gRPC on Broker: handled — `CheckPollerConfig` RPC. The multi-poller extension (per-poller sections plus a global one) remains.
* in the resources table, we currently only have poller_id, is it wise to also add zone_id? First impression: yes. Even if overall we replace poller_id with zone_id, there are exceptions!! Poller IDs keep their meaning for example to access logs.

# Issue resolution
## Moving external command sending to Broker
We create entry points for all Engine external commands on Broker.
And Broker, internally, sends the request to the concerned poller.

The entry point is the generic `ExecuteExternalCommand(ExternalCommandRequest
{ command })` RPC of `brokerrpc`: it receives the legacy line exactly as PHP already
produces it for the command pipe. Broker reads only the command name and the host
name (first argument), resolves the owning poller in its cache and pushes the whole
line, prefixed with the timestamp, in the `pb_external_command` BBDO event on that
poller's downward channel (the same one as `pb_notification_execute`, drained by the
poller stream's `read()`, i.e. under 100 ms). On the poller, `cbmod` hands it to the
pipe parser (`commands::processing::execute`): thread-safe commands
(`PROCESS_*_CHECK_RESULT`) run at once on the receiving thread, as the pipe reader
does, the others wait for the next event loop iteration. None of Engine's command
handlers is modified.

The routing table (`common/external_commands`) classifies each command by target:
process, global, host, service, hostgroup, servicegroup, contact, contactgroup,
downtime, comment. An Engine unit test checks it lists exactly the parser's commands.
Host and service go to the poller owning the host, with the `host_id` / `service_id`
resolved in the cache: Engine rewrites the names from the ids before parsing
(`processing::resolve_ids`), so the line may carry stale or empty names. Contact and
contactgroup are broadcast to every connected poller (each has its copy). Global and
process go to the poller named in the request (`PollerIdentifier`, mandatory: no
implicit broadcast of a `RESTART_PROGRAM`). Groups are no longer emitted by PHP, and
downtimes / comments designated by id stay `UNIMPLEMENTED`. In
`notification_mode = broker`, the commands Broker owns (acknowledgements, downtimes,
comments, notification switches) are **executed by Broker itself**: `legacy_commands`
converts the positional arguments into the typed request and the existing RPC method is
called as a plain method, no business logic duplicated
(`broker_impl::_execute_native_command`). Routed to the poller, they would be silently
ignored by Broker. Only downtime deletions by criteria, which have no typed RPC, stay
`UNIMPLEMENTED`.

Test coverage of the routing comes for free from the Robot tooling: every legacy line
the keywords of `Engine.py` emit (`Ctn Process Service Check Result`, `Ctn Acknowledge
Service Problem`, `Ctn Disable Host Check`, ...) goes through a single helper,
`Ctn Send External Command`, which writes the line to the poller command pipe by default
and hands it to `ExecuteExternalCommand` when the Robot variable `${EXTCMD_MODE}` is
`broker` (`Ctn Set External Command Mode    broker` in a test, or
`--variable EXTCMD_MODE:broker` for a whole campaign). The BBDO3 tests of
`external-commands.robot`, `external-commands2.robot` and `acknowledgement.robot` run in
broker mode, their BBDO2 twins keep the pipe, so both paths stay covered without
doubling the campaign.

> For the PHP side of moving external commands to gRPC and the Engine/Broker routing
> rule based on `notification_mode`, see
> [PHP evolutions — External commands over gRPC](../php-evolutions-en.md#evolution-2--external-commands-over-grpc).
