# Broker documentation {#mainpage}

## Table of content

* [Processing](#Processing)
  * [Feeder](#Feeder)

* [BAM](#BAM)
  * [Events in BAM](#EventsinBAM)
  * [Impact BA](#ImpactBA)
  * [Best BA](#BestBA)
  * [Worst BA](#WorstBA)
  * [Ratio Number BA](#RatioNumberBA)
  * [Ratio Percent BA](#RatioPercentBA)


## Processing

There are two main classes in the broker Processing:

* **failover**: This is mainly used to get events from broker and send them to a
  stream.
* **feeder**: This is mainly the reverse of a failover. Data are read from a stream
  and published into broker. This class provides a mechanism of retention to
  keep events until they are handled correctly.

### Feeder

A feeder has two roles:

1. The feeder can read events from its muxer. This is the case with a reverse
   connections, the feeder gets its events from the broker engine through the
   muxer and writes them to the stream client.

2. The feeder can also read events from its stream client. This is more usual.


#### Initialization

A feeder is created with a static function `feeder::create()`. This function:

* calls the constructor.
* starts the statistics timer
* starts its main loop.

The main loop runs with a thread pool managed by ASIO so don't expect to see
an std::thread somewhere.

A feeder is initialized with:

* name: name of the feeder.
* client: the stream to exchange events with.
* read\_filters: read filters, that is to say events allowed to be read by the
  feeder. Events that don't obey to these filters are ignored and thrown away
  by the feeder.
* write\_filters: same as read filters, but concerning writing.

After the construction, the feeder has its statistics started.
Statistics are handled by an ASIO timer, every 5s the handler `feeder::_stat_timer_handler()` is called.

Then, it is time for the feeder to start its main loop.
the `feeder::_read_from_muxer()` method is called and this last one will be called until the end of the feeder.

And there is a last loop to start, the one concerning stream reading. The feeder constructor calls `feeder::_start_read_from_stream_timer()` that starts a timer, each time its duration is reached, the `feeder::_read_from_stream_timer_handler()` method is called.

#### Reading the muxer

Let's describe a little more the `feeder::_read_from_muxer()` method and its mechanisms.

When called, this function:

* The feeder mutex is locked: then if a second call to this function call arrives, it will wait.
* creates a vector and initializes its size with the number of events in the muxer queue.
* if the state of the feeder is not set to running, the function execution is interrupted.
* the main loop of the method is then started here, it will be stopped on timeout, on a the feeder interruption or if there are no more events to read.
* this loop calls a `muxer::read()` asynchronous method. This method tries to fill the vector with as many events as it can store in it. If there is not suffisantly events, it keeps a callback so it will be ready to fill it again when new events will arrive. This method returns **true** if there are still events to send, otherwise it returns **false**.
* if some events have been retrieved, they are written to the feeder stream.
* Some checks on errors are made.
* The loop continues until one of its conditions is true.
* And if we have to continue, the function is post again to the ASIO mechanism.

#### Reading the stream

The method used here is `feeder::_read_from_stream_timer_handler()`.

While events are not null, they are pushed into a list.
Once this is done, this list is published to the muxer (specific muxer method used
for that `muxer::write(std::list<std::shared_ptr<io::data>>&)` and a new call to
`feeder::_start_read_from_stream_timer()` is made. And the loop starts again.

#### Concurrency

Events order is very important. So we can not make two calls to the `_read_from_muxer` method at the same time. It is almost the same when reading from the stream.

The easiest way was then to lock the feeder mutex

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

