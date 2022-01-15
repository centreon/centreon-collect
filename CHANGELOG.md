# Changelog

## 21.10.1

### centreon-broker

#### Fixes

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

*mysql_connection*

A timeout is added on mysql\_ping and this function is less called than before.

*tls*

Printing encrypted write logs on trace level only.

### centreon-engine

#### Fixes

*modules*

Fixing segfault.
