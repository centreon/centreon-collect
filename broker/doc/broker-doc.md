# Broker documentation {#mainpage}

## Architecture

### Static instances

Broker works with several singletons, `broker::engine`, `common::log\_v2` are
such examples among others. These objects are **not** declared as shared
pointers and should **not** become as such. If they are singletons, they can
be called from everywhere and at everytime in the code. The shared pointer has
a counter telling how many users hold it, when this counter is 0, it is removed.
But in case of a static instance, we can still call the instance() object which
became nullptr, otherwise we should test everywhere that the pointer exists
which would be an aberation.

So static instances are created at the start of cbd and destroyed at the end.
This is essentially done using those calls:

* `config::applier::init(conf);`
* `config::applier::deinit();`

### Shared pointers

Broker works also with shared pointers. They are very useful during asynchronous
calls ; cbd is heavily based on boost::asio. When a callback is posted to asio,
it is very easy to capture shared pointer, so we are sure they are still alive
when the function is executed.

But we have to be careful with static instances since they are not shared
pointers: Broker cannot stop before all the asynchronous calls are finished.
The following steps must be done in the following order:

1. Construction of static instances.
2. Construction of shared pointers and life of cbd
3. Asio work stopped correctly.
4. When Asio is completely stopped, destruction of static instances.

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

## Multiplexing

Each feeder and failover contains one or more muxer. This is this object that
has a queue to stack events.

A muxer can receive data from its associated stream or from the other side:
others muxers. And it can also send data to the same peers. The object used to
connect all the muxers is the multiplexing::engine.

### multiplexing::engine

This class has a unique instance initialized with the static method `load()`.
And to destroy this instance, we use the static method `unload()`.

The engine instance is a shared pointer to make easier its use with asynchronous functions call.

#### publish

When a muxer receives data from a stream, it publishes it to the engine. The goal here is to transmit data to all the muxers attached to the engine. This is done with the `engine::publish()` method we can find in two declinations:

* A first one with only one argument
* A second one with an `std::deque` of events

The second works faster than the first one as we send data in bulk.

The engine has an internal queue named `_kiew` that is an `std::deque` of shared pointers of events.

The engine has three possible states that are:

* **not\_started** The state is applied only just after the `load()` function and before it is changed to **running** or **stopped**. If some events are published while the engine is *not started*, these events are just stacked in the engine.
* **running** Usually this is the commonly used state. Events are also stacked on the queue, but then they are sent to subscribers (the muxers).
* **stopped** The state just after the call to the `stop()` method. Events published to the engine while it is *stopped*, are directly stored in a retention file with the *unprocessed* extension. These events will be played when **cbd** will restart. With this state, the engine doesn't use its internal queue.

`engine::publish()` stacks events in *_kiew* and then calls the internal function `engine::_send_to_subscribers()` which we are going to describe here. This last method can be called with a callback or type `std::function<void()>`.

In this function, we check **_sending_to_subscribers**, if it is true, the function returns, otherwise it was **false** and we set it to **true**.

If there are no muxer or the current **_kiew** is empty, then **_sending_to_subscribers** is set to **false** and the function return **false**.

Now, we are in the case where there are muxers and a non empty **_kiew**. A local kiew is created and we swap the contents of the two queues.

A callback_caller is instantiated with the given lambda. Its goal is to be called when the publication to muxers is over. And at this point, it calls the lambda.

The only possible callback is a lambda with a promise. Its goal is to make the publish method to be synchronous, it is useful when the `engine::stop()` method is called.

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

