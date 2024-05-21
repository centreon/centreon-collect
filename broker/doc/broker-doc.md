# Broker documentation {#mainpage}

## Table of content

- [Broker documentation {#mainpage}](#broker-documentation-mainpage)
  - [Table of content](#table-of-content)
  - [BBDO events](#BbdoEvents)
  - [Multiplexing](#Multiplexing)
    - [Muxer](#Muxer)
    - [Engine](#Engine)
  - [Processing](#processing)
    - [Feeder](#feeder)
      - [Initialization](#initialization)
      - [Reading the muxer](#reading-the-muxer)
      - [Reading the stream](#reading-the-stream)
      - [Concurrency](#concurrency)
  - [BAM](#bam)
    - [Events in BAM](#events-in-bam)
    - [Impact BA](#impact-ba)
    - [Best BA](#best-ba)
    - [Worst BA](#worst-ba)
    - [Ratio number BA](#ratio-number-ba)
    - [Ratio percent BA](#ratio-percent-ba)
    - [BAM cache](#bam-cache)
  - [Modules](#modules)
    - [grpc module](#grpc-module)
      - [caution](#caution)
      - [Main classes](#main-classes)
      - [generate\_proto.py](#generate_protopy)

## BBDO events
Broker is a software that gets events on some inputs and sends them on some outputs. We
can apply filters on all these inputs/outputs.

Events are divided by categories, they are:
  * *neb:* For Nagios Event broker. They are the main events received from Engine.
  * *bbdo:* They are the cops of the BBDO events. Essentially created by Broker to
    control others events.
  * *storage:* This category represents all the events created by the sql streams
    after transformation of some neb events.
  * *dumper:* not used.
  * *bam:* the bam events.
  * *extcmd:* If you use the Broker API, then the sent commands will generate
    such category events.
  * *generator:* not used.
  * *local:* That is an internal category. **Events here can not pass through
    network**. They stay inside the generator software.
  * *internal:* Others cops events. They are very few.

## Multiplexing
### Muxer
A muxer can be plugged to an input or an output stream.
Muxers exchange data through the Engine object. Each muxer holds a shared pointer to the Engine. So the Engine is destroyed when all the muxers are removed from Broker.

We can send a data `d` to the muxer by using the `write(d)` method. With that method, the Muxer publishes to the Engine the data `d`. And then the Engine stacks on its queue the event. In case, the Engine is running, the task finishes by calling an internal function `_send_to_subscribers()` that empties the queue and sends all the data to each muxer on its own queue.

So, when a data is written to a muxer, it is stacked on all the muxers using the Engine, even the muxer at the origin of the data.

The `_send_to_subscribers()` internal function is a complex function. It can take a callback as argument. Let's take a look at it.

Firstly, it checks if `_sending_to_subscribers` is `true`, that means the action of sending data to subscribers is already running and the function returns.

If `_sending_to_subscribers` is `false`, it is changed to `true` and a copy of the Engine queue is made as a shared pointer to give it to each muxer. To send that queue, the thread pool is used and its task is just to call the `publish()` method of the muxers to push the events on their stack.
In case of a callback is given as argument, it will be called once all the tasks are finished.

This callback is rarely used. It is essentially used when we want to be sure that no more data are sent to the subscribers. In this context, the callback is just a promise to wait until all the tasks are finished. We can see this in:
* the `stop()` method of Engine: If we want to stop it, we must be sure that no events have to be written somewhere.
* the `unsubscribe_muxer()` method of Engine: This method removes the muxer from the Engine list of subscribers. So if we remove one muxer from it but there are still events to write into it, we have the risk of an access to a removed muxer.

#### Muxer filter

To limit data in muxers, they work with filters. Each muxer is created with two filters:

* mandatory filter: it contains data types mandatory for the good work of the muxer. Each stream defines its mandatory filter, and when the muxer is created, this filter is transferred to the muxer.
* forbidden filter: This filter declares what event type we don't want to see in the muxer

From these two filters, and with the configuration given by the user, a map of filters is defined in the muxer and each time an event arrives, we check that the event is authorized to go through the muxer.

If the user allows events that make part of the forbidden filter, an error message is raised in the logs but the events are forbidden to be coherent with the forbidden filter. If the user doesn't declare events that are mandatory, broker will add them into the filter to allow them.

### Engine

The Broker engine is hold by its subscriber muxers. Each one has a shared pointer
to it. So the engine can be removed only when there are no more muxers.

The engine has an array of weak pointers to these muxers. So when it is time
to stop it, we wait for each muxer to be removed from the array. This is produced
when the muxers themselves are stopped. If there are still data in the engine, they
are saved to an *unprocessed* file that will be loaded on next Broker start.

The engine class is the root of events dispatching. Events arrive from a stream,
are transfered to a muxer and then to engine (at the root of the tree).
This one then sends events to all its children. Each muxer receives these events
and sends them to its stream.
 
## Processing

There are two main classes in the Broker processing:

* **failover**: This is mainly used to get events from broker and send them to a
  stream.
* **feeder**: This is mainly the reverse of a failover. Data are read from a stream
  and published into broker. This class provides a mechanism of retention to
  keep events until they are handled correctly.

### Feeder

A feeder has two roles:

1. The feeder can read events from its muxer. This is the case with a reverse connections, the feeder gets its events from the Broker engine through the muxer and writes them to the stream client.

2. The feeder can also read events from its stream client. This is more usual.


#### Initialization

A feeder is created with a static function `feeder::create()`. This function:

* calls the constructor.
* starts the statistics timer
* starts its main loop.

The main loop runs with a thread pool managed by ASIO so don't expect to see an std::thread somewhere.

A feeder is initialized with:

* `name`: name of the feeder.
* `client`: the stream to exchange events with.
* `read_filters`: read filters, that is to say events allowed to be read by the
  feeder. Events that don't obey to these filters are ignored and thrown away
  by the feeder.
* `write_filters`: same as `read_filters`, but concerning writing.

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

### BAM cache
When BAM is stopped (broker is stopped or reloaded), living data are saved into a cache. There are two kinds of information:
* InheritedDowntime: it is then possible to restore the exact situation of the BA's concerning downtimes when cbd will be restarted.
* ServicesBookState: the goal of this message is to save the BA's states. This message contains only services' states as they are the living parts of BA's. And these services states are minimalistic, we just save data used by BAM.

## Modules

### grpc module

#### caution

grpc threads block at shutdown if grpc object aren't cleanly stopped. For example, we must call ClientBeReactor::Finish before delete. That's why grpc::stream::stop must be called before destruction (shared_ptr< stream >::reset()). So be careful to not forget a case (a catch handler).
Another issue: channel is owned both by connector and client stream context. At shutdown, if the last owner is client stream object, client destructor is called by OnDone method called by a grpc thread. Then channel destructor is called by grpc thread. The issue is that channel destructor tries to join current grpc thread. The solution is to leave stream destruction job to asio threads.

#### Main classes

Module works in streaming mode. So every request, response use asynchronous bireactors.
The two bireactors provided by grpc have the same method StartRead, StartWrite, OnWriteDone, OnReadDone and OnDone. We only can delete reactors object once OnDone has been called. 
As this object inherited by streams are also referenced by failovers and as grpc doesn't rely enough on shared_ptr in my taste, every shared_ptr< bireactor > is stored in a static set to manage object lifetime.

* stream: this templated class inherit either from an ClientBiReactor or ServerBiReactor. It's created by server service exchange method or connector
* acceptor: it contains only a grpc server and a grpc service with exchange method
* connector: it creates a channel that remains alive during connector lifetime even if we face to network issues. This channel will be used by stream clients.

#### generate_proto.py

This module works in two modes
*bbdo mode: in this mode all events are encoded by bbdo layer, and datas are stored in CentreonEvent.buffer (bytes array)
*direct mode: bddo layer is bypassed. So CentreonEvent must have in is one of all bbdo events. This is the purpose of this script, it scans all proto files that we find in bbdo directory. It then generates a proto file with a one of that contains all these events. It also generates grpc_bridge.cc that contains two functions, one that store bbdo event in OneOf of CentreonEvent and another that extract bbdo event from CentreonEvent
A part of generated code: 
```c++
/**
 * @brief this function creates a io::protobuf_object from grpc received message
 *
 * @param stream_content message received
 * @return std::shared_ptr<io::data> shared_ptr<io::protobuf<xxx>>, null if
 * unknown content received
 */
std::shared_ptr<io::data> protobuf_to_event(const event_ptr & stream_content) {
    switch(stream_content->content_case()) {
        case ::stream::CentreonEvent::kServicesBookState:
            return std::make_shared<detail::received_protobuf<
                ServicesBookState, make_type(io::bam, bam::de_pb_services_book_state)>>(
                stream_content, &grpc_event_type::servicesbookstate_,
                &grpc_event_type::mutable_servicesbookstate_);
        case ::stream::CentreonEvent::kWelcome:
            return std::make_shared<detail::received_protobuf<
                Welcome, make_type(io::bbdo, bbdo::de_welcome)>>(
                stream_content, &grpc_event_type::welcome_,
                &grpc_event_type::mutable_welcome_);
                ......
        default:
      SPDLOG_LOGGER_ERROR(log_v2::grpc(), "unknown content type: {} => ignored",
                          static_cast<uint32_t>(stream_content->content_case()));
      return std::shared_ptr<io::data>();
    }
}


/**
 * @brief this function create a event_with_data structure that will be send on grpc.
 * stream_content don't have a copy of event, so event mustn't be
 * deleted before stream_content
 *
 * @param event to send
 * @return object used for send on the wire
 */
std::shared_ptr<event_with_data> create_event_with_data(const std::shared_ptr<io::data> & event) {
    std::shared_ptr<event_with_data> ret;
    switch(event->type()) {
        case make_type(io::bam, bam::de_pb_services_book_state):
            ret = std::make_shared<event_with_data>(
                event, reinterpret_cast<event_with_data::releaser_type>(
                &grpc_event_type::release_servicesbookstate_));
            ret->grpc_event.set_allocated_servicesbookstate_(&std::static_pointer_cast<io::protobuf<ServicesBookState, make_type(io::bam, bam::de_pb_services_book_state)>>(event)->mut_obj());
            break;

        case make_type(io::bbdo, bbdo::de_welcome):
            ret = std::make_shared<event_with_data>(
                event, reinterpret_cast<event_with_data::releaser_type>(
                &grpc_event_type::release_welcome_));
            ret->grpc_event.set_allocated_welcome_(&std::static_pointer_cast<io::protobuf<Welcome, make_type(io::bbdo, bbdo::de_welcome)>>(event)->mut_obj());
            break;

    default:
        SPDLOG_LOGGER_ERROR(log_v2::grpc(), "unknown event type: {}", *event);
    }
    if (ret) {
        ret->grpc_event.set_destination_id(event->destination_id);
        ret->grpc_event.set_source_id(event->source_id);
    }
    return ret;
}

```
