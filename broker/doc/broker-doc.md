# Broker documentation {#mainpage}

## BAM

There are five types of BA.
* impact BA
* best BA
* worst BA
* ratio number BA
* ratio percent BA

Before describing these BA, let's try to explain how they lives and how they are computed.

All the BA classes derived from an abstract `ba` class.
They have to implement the following methods:
* `bool _apply_impact(kpi*, impact_info&)`: knowing that the `kpi*` given in parameter is a child of the BA, this method applies on the BA the impact of the KPI, knowing its change stored in the `impact_info` object. If this changes the BA, the method must return `true` to be sure impacts are propagated to parents, otherwise it returns `false`.
* `void _unapply_impact(kpi*, impact_info&)`: the method is the reverse of `_apply_impact()`.
* `std::string get_output() const`: builds the output string of the Engine virtual service relied to this BA.
* `std::string get_perfdata() const`: builds the performance data for the virtual service relied to this BA.
* `state get_state_hard() const`: returns the current state of the BA (an enum corresponding to service states, one value among `state_ok`, `state_warning`, `state_critical` or `state_unknown`).
* `state get_state_soft() const`: the same as the previous one but for soft states. This method is currently not used.

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

### BAM cache
When BAM is stopped (broker is stopped or reloaded), living data are saved into a cache. There are two kinds of information:
* InheritedDowntime: it is then possible to restore the exact situation of the BA's concerning downtimes when cbd will be restarted.
* ServicesBookState: the goal of this message is to save the BA's states. This message contains only services' states as they are the living parts of BA's. And these services states are minimalistic, we just save data used by BAM.