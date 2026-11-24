Negotiation between Engine and Broker
=====================================

# Introduction

This document describes how `Engine` and `Broker` work together in BBDO 3 since the arrival of the **centralized configuration**, and how this groundwork prepares the **Poller HA**.

## Where we started

Originally, PHP generated the configuration of each poller from the configuration database, and Gorgone dropped it on the poller. At each startup, `Engine` read its `centengine.cfg` and then sent its whole configuration back to `Broker`, which used it to prepare the real-time database. `Engine` was also the only one to hold the runtime state: check states, downtimes, comments, acknowledgements, notifications. This state was saved in a `retention.dat` local to each poller. A resource was therefore tied to its poller, and the central server knew nothing it had not been sent first.

## What exists today

Two modes coexist, chosen per poller (see [Overview: who handles what](01-negotiation-en.md#overview-who-handles-what-depending-on-configuration)):

* **legacy**: nothing changes compared to the historical situation;
* **centralized** (BBDO ≥ 3, `cache_config_directory`, `Engine` started with `-p`). `Broker` owns the configuration of the pollers:
  * PHP writes the configuration of each poller in a directory and announces it with a `.lck` file;
  * `Broker` reads it, validates it, including the dependencies between pollers, and keeps it serialized in `<ID>.prot`;
  * it computes the difference and sends it to `Engine` as a `DiffState`;
  * `Engine` applies it and keeps a copy in `state.prot`.

  If pollers are attached to a remote server, the `Broker` of that remote server acts as a relay and forwards these exchanges to the central server.

`Broker` has thus become the keeper of the state. Its global cache (`broker_cache`), persisted on disk, knows every resource of every poller. When `Engine` connects, `Broker` sends it back its runtime state (`RuntimeState`), which replaces `retention.dat`. With `notification_mode = broker`, `Broker` also owns downtimes, acknowledgements, comments and the notification decision. External commands can go through `Broker`, which routes them to the right poller. BAM runs in `Broker` and reloads when the configuration changes.

## Where we are going

All this work is the set of prerequisites for the Poller HA. The first milestone is an active/passive redundancy with static placement. `Broker` remains a single point of failure for this milestone, a deliberate choice. The target architecture is described in [Target architecture — toward poller HA](../ha-target-architecture-en.md).

## Document map

| Chapter | Content |
|---|---|
| [New negotiation](01-negotiation-en.md#new-negotiation) | `cbmod` as a library, parameters, `Welcome` exchange |
| [Reading Engine configuration](02-configuration-delivery-en.md#reading-engine-configuration) | announcement, preparation, delivery and acknowledgement, configuration round, directory watching |
| [Validating and writing the configuration](03-configuration-validation-en.md#validating-and-writing-the-configuration) | cross-poller validation, `CheckPollerConfig`, computing the difference, database writes |
| [Broker centralized cache](04-centralized-cache-en.md#broker-centralized-cache) | feeding, persistence, legacy mode |
| [Retention and RRD stream](05-retention-rrd-en.md#retention-and-rrd-stream) | catching up a long retention without blocking real time |
| [Remote Servers](06-remote-servers-en.md#remote-servers-and-centralized-configuration) | relays, topology, moving a poller between remotes |
| [Downtimes and acknowledgements](07-downtimes-acknowledgements-en.md#centralized-downtime-and-acknowledgement-management) | `notification_mode`, notification decided by `Broker` |
| [BAM in Broker](08-bam-en.md#bam-in-broker) | current behavior in both modes |
| [Preparatory work](09-ha-preparatory-work-en.md#preparatory-work-before-poller-ha) | the five HA prerequisites, including the removal of `retention.dat` |
| [Poller HA](10-poller-ha-en.md#poller-ha) | zones, assignment, distribution, rebalancing |
| [Tickets and issues](11-tickets-and-issues-en.md#tickets) | splitting into tickets, potential issues and their resolution |

# Contents

<!-- TOC -->
* [New negotiation](01-negotiation-en.md#new-negotiation)
  * [cbmod becomes a library](01-negotiation-en.md#cbmod-becomes-a-library)
  * [Engine startup parameters](01-negotiation-en.md#engine-startup-parameters)
  * [Broker parameters](01-negotiation-en.md#broker-parameters)
  * [Overview: who handles what, depending on configuration](01-negotiation-en.md#overview-who-handles-what-depending-on-configuration)
  * [The negotiation](01-negotiation-en.md#the-negotiation)
* [Reading Engine configuration](02-configuration-delivery-en.md#reading-engine-configuration)
  * [Configuration management on the Engine side](02-configuration-delivery-en.md#configuration-management-on-the-engine-side)
  * [Managing configuration sending by Broker to Engine](02-configuration-delivery-en.md#managing-configuration-sending-by-broker-to-engine)
  * [Some more technical points](02-configuration-delivery-en.md#some-more-technical-points)
  * [Split of broker::config::applier::state](02-configuration-delivery-en.md#split-of-brokerconfigapplierstate)
* [Validating and writing the configuration](03-configuration-validation-en.md#validating-and-writing-the-configuration)
  * [Cross-poller validation: `load_foreign_objects()`](03-configuration-validation-en.md#cross-poller-validation-load_foreign_objects)
  * [Validating a poller configuration: the `CheckPollerConfig` gRPC endpoint](03-configuration-validation-en.md#validating-a-poller-configuration-the-checkpollerconfig-grpc-endpoint)
  * [Calculating the difference](03-configuration-validation-en.md#calculating-the-difference)
  * [Writing configuration to database](03-configuration-validation-en.md#writing-configuration-to-database)
  * [Tricky cases](03-configuration-validation-en.md#tricky-cases)
  * [Cross-cutting objects](03-configuration-validation-en.md#cross-cutting-objects)
  * [The necessity of centralized cache](03-configuration-validation-en.md#the-necessity-of-centralized-cache)
* [SQL/storage streams](04-centralized-cache-en.md#sqlstorage-streams)
* [Broker centralized cache](04-centralized-cache-en.md#broker-centralized-cache)
  * [Feeding the cache](04-centralized-cache-en.md#feeding-the-cache)
  * [What is persisted at shutdown](04-centralized-cache-en.md#what-is-persisted-at-shutdown)
  * [Host `poller_id` population in the cache](04-centralized-cache-en.md#host-poller_id-population-in-the-cache)
  * [*Legacy* mode](04-centralized-cache-en.md#legacy-mode)
  * [Possible evolutions](04-centralized-cache-en.md#possible-evolutions)
* [Retention and RRD stream](05-retention-rrd-en.md#retention-and-rrd-stream)
  * [Current problem](05-retention-rrd-en.md#current-problem)
  * [Proposed architecture](05-retention-rrd-en.md#proposed-architecture)
  * [Buffer format](05-retention-rrd-en.md#buffer-format)
  * [Multiple disconnections](05-retention-rrd-en.md#multiple-disconnections)
  * [Integration with the existing rebuild](05-retention-rrd-en.md#integration-with-the-existing-rebuild)
  * [Edge cases](05-retention-rrd-en.md#edge-cases)
  * [Concurrency](05-retention-rrd-en.md#concurrency)
  * [Migration plan](05-retention-rrd-en.md#migration-plan)
* [Remote Servers and centralized configuration](06-remote-servers-en.md#remote-servers-and-centralized-configuration)
  * [Current situation](06-remote-servers-en.md#current-situation)
  * [Relay identification](06-remote-servers-en.md#relay-identification)
  * [New BBDO messages](06-remote-servers-en.md#new-bbdo-messages)
  * [Topology in broker\_cache](06-remote-servers-en.md#topology-in-broker_cache)
  * [Topology persistence](06-remote-servers-en.md#topology-persistence)
  * [Scenario 1: poller connects to the remote](06-remote-servers-en.md#scenario-1-poller-connects-to-the-remote)
  * [Scenario 1b: relay chain (multi-hop)](06-remote-servers-en.md#scenario-1b-relay-chain-multi-hop)
  * [Scenario 2: configuration pushed by PHP](06-remote-servers-en.md#scenario-2-configuration-pushed-by-php)
  * [Scenario 3: remote offline then reconnects](06-remote-servers-en.md#scenario-3-remote-offline-then-reconnects)
  * [Scenario 4: central restart](06-remote-servers-en.md#scenario-4-central-restart)
  * [Poller migration between two remotes](06-remote-servers-en.md#poller-migration-between-two-remotes)
  * [gRPC GetTopology endpoint](06-remote-servers-en.md#grpc-gettopology-endpoint)
  * [.prot file storage](06-remote-servers-en.md#prot-file-storage)
  * [broker\_state evolution](06-remote-servers-en.md#broker_state-evolution)
  * [Required changes](06-remote-servers-en.md#required-changes)
  * [Roll-out](06-remote-servers-en.md#roll-out)
* [Centralized downtime and acknowledgement management](07-downtimes-acknowledgements-en.md#centralized-downtime-and-acknowledgement-management)
  * [Problem](07-downtimes-acknowledgements-en.md#problem)
  * [Solution: the notification_mode parameter](07-downtimes-acknowledgements-en.md#solution-the-notification_mode-parameter)
  * [Persistence](07-downtimes-acknowledgements-en.md#persistence)
  * [Migration and downtimes / acknowledgements](07-downtimes-acknowledgements-en.md#migration-and-downtimes--acknowledgements)
  * [Driving the notification decision on Broker](07-downtimes-acknowledgements-en.md#driving-the-notification-decision-on-broker)
* [BAM in Broker](08-bam-en.md#bam-in-broker)
  * [The objects](08-bam-en.md#the-objects)
  * [Two streams, one module](08-bam-en.md#two-streams-one-module)
  * [Starting up, in both regimes](08-bam-en.md#starting-up-in-both-regimes)
  * [Circular definitions](08-bam-en.md#circular-definitions)
  * [What was measured and where](08-bam-en.md#what-was-measured-and-where)
* [Preparatory work before Poller HA](09-ha-preparatory-work-en.md#preparatory-work-before-poller-ha)
  * [Prerequisite: timeperiods must be Broker-manageable](09-ha-preparatory-work-en.md#prerequisite-timeperiods-must-be-broker-manageable)
  * [Triple-priority neb queue](09-ha-preparatory-work-en.md#triple-priority-neb-queue)
  * [BAM: reading downtime state from the Broker cache](09-ha-preparatory-work-en.md#bam-reading-downtime-state-from-the-broker-cache)
  * [Implementing notification_mode = broker on single-poller zones](09-ha-preparatory-work-en.md#implementing-notification_mode--broker-on-single-poller-zones)
  * [Test strategy](09-ha-preparatory-work-en.md#test-strategy)
  * [Prerequisite 5: removing `retention.dat`](09-ha-preparatory-work-en.md#prerequisite-5-removing-retentiondat)
* [Poller HA](10-poller-ha-en.md#poller-ha)
  * [Poller configuration tree](10-poller-ha-en.md#poller-configuration-tree)
  * [Engine self-monitoring](10-poller-ha-en.md#engine-self-monitoring)
  * [HA protocol architecture](10-poller-ha-en.md#ha-protocol-architecture)
* [Tickets](11-tickets-and-issues-en.md#tickets)
  * [First tickets](11-tickets-and-issues-en.md#first-tickets)
  * [Multiple tickets in parallel](11-tickets-and-issues-en.md#multiple-tickets-in-parallel)
  * [Evolutions](11-tickets-and-issues-en.md#evolutions)
* [Potential issues to resolve](11-tickets-and-issues-en.md#potential-issues-to-resolve)
* [Issue resolution](11-tickets-and-issues-en.md#issue-resolution)
  * [Moving external command sending to Broker](11-tickets-and-issues-en.md#moving-external-command-sending-to-broker)
<!-- TOC -->
