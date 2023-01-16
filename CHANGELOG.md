# Changelog

## 21.10.3

### centreon-broker

#### Enhancements

*grpc*

The grpc api listens by default on localhost now. And it can be configured
with the Broker configuration file.

#### Fixes

*core*

A possible deadlock has been removed from stats center.

*rrd*

Rebuild of graphs should work better.

*bam*

If a service with two overlapping downtimes is a BA kpi. When the first downtime
is cancelled from the service, it is as if all the downtimes are removed from
the kpi. This new version fixes this issue.

### centreon-engine

#### Enhancements

*grpc*

The grpc api listens by default on localhost now. And it can be configured
with the Engine configuration file.

## 21.10.2

### centreon-broker

#### Fixes

*core*

Unknown filters applied in the configuration file do not hang broker anymore.
Improve future usage

*log*

If a logger was at the 'off'/'disabled' state, then broker did not start.

*sql*

Hostgroups and servicegroups are deleted 5s after the last instance restart.

Size of notes\_url, notes and action\_url columns are reviewed to match the
web configuration.

*tcp*

*service status*

Connections are consumed in the same order they are created.

*downtime*

The downtime full delete external command did not work correctly and filtered
too much.

#### Improvements

*lua*

New function `broker.bbdo_version()` implemented in broker streamconnector.
stream connector accepts empty parameters

### centreon-engine

Service status could be sent twice to cbd. This is fixed now.

### centreon-connector

#### Fixes
*perl connector*

Connector multiplexer rewritten using ASIO.

## 21.10.1

### centreon-broker

#### Fixes

*lua*

manage new perfdata labels in Broker's perfdata parsing routine

*multiplexing*

The multiplexing engine works now asynchronously compared to its muxers. This
improves a lot performances.

*rrd*

Add SQL query to check metrics to delete with their associated files.

*storage*

Waiting longer for conflict manager to be connected without blocking if cbd
is stopped.

Cache loading queries are parallelized.

*bam*

A ba configured with best status was initialized in state OK. And when KPI were
added, their status were always worst than OK (best case was OK). So even if all
the kpi were in critical state, the state appeared as OK.

The cache in bam is lighter, metrics are no
more loaded. And queries to load the cache are parallelized.

Inherited downtimes were duplicated on cbd reload. This is fixed.

*mysql_connection*

* A timeout is added on mysql\_ping and this function is less called than
  before.
* gRPC stats are improved on connections.

*tls*

Printing encrypted write logs on trace level only.

### centreon-engine

#### Fixes

*modules*

Fixing segfault.
