# Broker documentation {#mainpage}

## BAM

There are five types of BA.
* impact BA
* best BA
* worst BA
* ratio number BA
* ratio percent BA

Before describing these BA, let's try to explain how they lives and how they are computed.

### Events in BAM

A BA has children which are KPIs. A KPI is a class which is derived in various classes `kpi_ba`, `kpi_service`, `kpi_boolexp`, etc...

A `kpi_ba` is a KPI owning a BA, a `kpi_service` is a KPI owning a service, etc.

To avoid to compute all a BA from the beginning after a change, BAs and some other objects are derived from classes:

* `service_listener`
* `computable`.

Then, for example, when a *service status* is received by the `monitoring_stream`, a call is made to the `service_book::update()` method. And service listeners are notified by the change in the concerned service.

A BA is a tree made of KPIs and *boolean rules*. If one of these objects implements the `service_listener` and is modified by such an event, it notifies its parents by calling `computable::notify_parents_of_change()`.
Modifications are then applied if needed on parents, and then they notify their own parents if they are changed, etc.

When the tree is built, it is important to know that each node knows:
* its parents
* its children.

### Impact BA

This is the first implemented BA.

A such BA starts with 100 points. Each KPI has an amount of points for a
WARNING or a CRITICAL state. When a KPI is critical state, the BA has its
points reduced of the corresponding amount of points.

The amount of a BA points is forced in the range [0;100].

Technically, an `impact_ba` class is derived from `ba`. The current amount of
points is in the `_level_hard` attribute.

The BA is considered in WARNING if `_level_critical < _level_hard =< _level_warning`.

The BA is considered OK if `_level_warning < _level_hard`.

The BA is considered CRITICAL if `_level_hard <= _level_critical`.

### Best BA

This BA has its state set to the best among all its KPIs.

Technically, its class `ba_best` is derived from `ba`.

### Worst BA

This BA has its state set to the worst among all its KPIs.

Technically, its class `ba_worst` is derived from `ba`.

### Ratio number BA

This BA computes the number of KPIs in state OK. It is possible to configure
a warning level and a critical level.

If the number of KPIs in state CRITICAL is less than the warning level, the BA is OK.
If this number is greater than WARNING but not than CRITICAL, the BA is WARNING.
Otherwise, the BA is CRITICAL.

Technically, a `ba_ratio_number` class is derived from `ba`.

The number of KPIs in state CRITICAL is stored in the `_level_hard` attribute.
Critical and warning levels are stored in `_level_critical` and `_level_warning`
attributes.

### Ratio percent BA

This BA works as the Ratio number BA, except that we count CRITICAL states
in percents relatively to the total of KPIs.

