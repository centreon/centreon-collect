/*
** Copyright 2011-2012,2015,2017, 2020-2021 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include "com/centreon/broker/processing/feeder.hh"

#include <unistd.h>

#include "com/centreon/broker/exceptions/connection_closed.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::processing;

constexpr unsigned max_event_queue_size = 0x10000;

/**
 *  feeder is an enable_shared_from_this, this static method ensures that object
 * is created by a new
 *
 *  @param[in] name           Name.
 *  @param[in] client         Client stream.
 *  @param[in] read_filters   Read filters.
 *  @param[in] write_filters  Write filters.
 */
std::shared_ptr<feeder> feeder::create(
    const std::string& name,
    std::shared_ptr<io::stream>& client,
    const multiplexing::muxer_filter& read_filters,
    const multiplexing::muxer_filter& write_filters) {
  std::shared_ptr<feeder> ret(
      new feeder(name, client, read_filters, write_filters));
  ret->_start_stat_timer();

  ret->_read_from_muxer();
  ret->_start_read_from_stream_timer();
  return ret;
}

/**
 *  Constructor.
 *
 *  @param[in] name           Name.
 *  @param[in] client         Client stream.
 *  @param[in] read_filters   Read filters.
 *  @param[in] write_filters  Write filters.
 */
feeder::feeder(const std::string& name,
               std::shared_ptr<io::stream>& client,
               const multiplexing::muxer_filter& read_filters,
               const multiplexing::muxer_filter& write_filters)
    : stat_visitable(name),
      _state{state::running},
      _client(std::move(client)),
      _muxer(multiplexing::muxer::create(name,
                                         std::move(read_filters),
                                         std::move(write_filters),
                                         false)),
      _stat_timer(pool::io_context()),
      _read_from_stream_timer(pool::io_context()),
      _io_context(pool::io_context_ptr()) {
  if (!_client)
    throw msg_fmt("could not process '{}' with no client stream", _name);

  set_last_connection_attempt(timestamp::now());
  set_last_connection_success(timestamp::now());
  set_state("connected");
  SPDLOG_LOGGER_DEBUG(log_v2::core(), "create feeder {}, {:p}", name,
                      static_cast<const void*>(this));
}

/**
 *  Destructor.
 */
feeder::~feeder() {
  SPDLOG_LOGGER_DEBUG(log_v2::core(), "destroy feeder {}, {:p}", get_name(),
                      static_cast<const void*>(this));

  multiplexing::engine::instance_ptr()->unsubscribe(_muxer.get());
}

bool feeder::is_finished() const noexcept {
  return _state == state::finished;
}

/**
 *  Get the read filters used by the feeder.
 *
 *  @return  The read filters used by the feeder.
 */
const std::string& feeder::_get_read_filters() const {
  return _muxer->read_filters_as_str();
}

/**
 *  Get the write filters used by the feeder.
 *
 *  @return  The write filters used by the feeder.
 */
std::string const& feeder::_get_write_filters() const {
  return _muxer->write_filters_as_str();
}

/**
 *  Forward to stream.
 *
 *  @param[in] tree  The statistic tree.
 */
void feeder::_forward_statistic(nlohmann::json& tree) {
  if (_protect.try_lock_for(std::chrono::milliseconds(300))) {
    _client->statistics(tree);
    _muxer->statistics(tree);
    _protect.unlock();
  }
}

/*****************************************************************************
 * muxer => client
 *****************************************************************************/
/**
 * @brief read event from muxer (both synchronous or asynchronous)
 * if an event is available => call _write_to_client
 * if not, _write_to_client will be called from muxer when event arrive
 */
void feeder::_read_from_muxer() {
  bool have_to_terminate = false;
  bool other_event_to_read = true;
  std::vector<std::shared_ptr<io::data>> events;
  std::chrono::system_clock::time_point timeout_read =
      std::chrono::system_clock::now() + std::chrono::milliseconds(100);
  std::unique_lock<std::timed_mutex> l(_protect);
  if (_state != state::running) {
    return;
  }
  events.reserve(_muxer->get_event_queue_size());
  while (other_event_to_read && !have_to_terminate &&
         std::chrono::system_clock::now() < timeout_read) {
    other_event_to_read =
        _muxer->read(events, max_event_queue_size,
                     [me = shared_from_this()]() { me->_read_from_muxer(); });

    SPDLOG_LOGGER_TRACE(log_v2::processing(),
                        "feeder '{}': {} events read from muxer", _name,
                        events.size());

    // if !other_event_to_read callback is stored and will be called as soon as
    // events will be available
    if (!events.empty()) {
      unsigned written = _write_to_client(events);
      if (written > 0) {
        _ack_event_to_muxer(written);
        // as retention may fill queue we try to read once more
        other_event_to_read = false;
      }
      if (written != events.size())  //_client fails to write all events
        have_to_terminate = true;
    }
    events.clear();
  }
  if (have_to_terminate) {
    _stop_no_lock();
    return;
  }
  if (other_event_to_read) {  // other events to read => give time to
                              // asio to work
    _io_context->post([me = shared_from_this()]() { me->_read_from_muxer(); });
  }
}

/**
 * @brief write event to client stream
 * _protect must be locked
 * @param event
 * @return number of events written
 */
unsigned feeder::_write_to_client(
    const std::vector<std::shared_ptr<io::data>>& events) {
  unsigned written = 0;
  try {
    for (const std::shared_ptr<io::data>& event : events) {
      if (log_v2::processing()->level() == spdlog::level::trace) {
        SPDLOG_LOGGER_TRACE(
            log_v2::processing(),
            "feeder '{}': sending 1 event {:x} from muxer to stream {}", _name,
            event->type(), *event);
      } else {
        SPDLOG_LOGGER_DEBUG(
            log_v2::processing(),
            "feeder '{}': sending 1 event {:x} from muxer to stream", _name,
            event->type());
      }
      _client->write(event);
      ++written;
    }
  } catch (exceptions::shutdown const&) {
    // Normal termination.
    SPDLOG_LOGGER_INFO(log_v2::core(), "from muxer feeder '{}' shutdown",
                       _name);
  } catch (const exceptions::connection_closed&) {
    set_last_error("");
    SPDLOG_LOGGER_DEBUG(log_v2::processing(), "feeder '{}' connection closed",
                        _name);
  } catch (const std::exception& e) {
    set_last_error(e.what());
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "from muxer feeder '{}' error:{} ", _name, e.what());
  } catch (...) {
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "from muxer feeder: unknown error occured while "
                        "processing client '{}'",
                        _name);
  }
  return written;
}

/**
 * @brief acknowledge event to the muxer and catch exception from it
 *
 * @param count
 */
void feeder::_ack_event_to_muxer(unsigned count) noexcept {
  try {
    _muxer->ack_events(count);
  } catch (const std::exception&) {
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        " {} fail to acknowledge {} events", _name, count);
  }
}

/**
 * @brief lock mutex and  stop feeder (timers, _muxer, _client)
 *
 */
void feeder::stop() {
  std::unique_lock<std::timed_mutex> l(_protect);
  _stop_no_lock();
}

/**
 * @brief stop feeder (timers, _muxer, _client) without locking
 *
 */
void feeder::_stop_no_lock() {
  state expected = state::running;
  if (!_state.compare_exchange_strong(expected, state::finished)) {
    return;
  }

  set_state("disconnected");

  // muxer should not receive events
  multiplexing::engine::instance_ptr()->unsubscribe(_muxer.get());
  _stat_timer.cancel();
  _read_from_stream_timer.cancel();

  /* We don't get back the return value of stop() because it has non sense,
   * the only interest in calling stop() is to send an acknowledgement to the
   * peer. */
  try {
    _client->stop();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "{} Failed to send stop event to client: {}", _name,
                        e.what());
  }
  SPDLOG_LOGGER_INFO(log_v2::core(),
                     "feeder: queue files of client '{}' removed", _name);
  _muxer->remove_queue_files();
  SPDLOG_LOGGER_INFO(log_v2::core(), "feeder: {} terminated", _name);
  // in order to avoid circular owning
  _muxer->clear_read_handler();
}

/**
 * @brief get muxer queue size
 *
 * @return uint32_t
 */
uint32_t feeder::_get_queued_events() const {
  return _muxer->get_event_queue_size();
}

/**
 * @brief let some delay to _client to write remain events
 *
 * @param ms_timeout time out in ms
 * @return true
 * @return false
 */
bool feeder::wait_for_all_events_written(unsigned ms_timeout) {
  std::unique_lock<std::timed_mutex> l(_protect);
  return _client->wait_for_all_events_written(ms_timeout);
}

/**
 * @brief every 5s stats are refreshed by _stat_timer_handler
 *
 */
void feeder::_start_stat_timer() {
  std::unique_lock<std::timed_mutex> l(_protect);
  _stat_timer.expires_from_now(std::chrono::seconds(5));
  _stat_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_stat_timer_handler(err);
      });
}

/**
 * @brief every 5s stats are refreshed by _stat_timer_handler
 *
 */
void feeder::_stat_timer_handler(const boost::system::error_code& err) {
  if (err || _state != state::running) {
    return;
  }
  set_queued_events(_muxer->get_event_queue_size());
  _start_stat_timer();
}

/*****************************************************************************
 * client  => muxer
 *****************************************************************************/

/**
 * @brief stream::read is synchronous so we call it every
 * idle_microsec_wait_idle_thread_delay with this timer and
 * _read_from_stream_timer_handler
 *
 */
void feeder::_start_read_from_stream_timer() {
  std::unique_lock<std::timed_mutex> l(_protect);
  _read_from_stream_timer.expires_from_now(
      std::chrono::microseconds(idle_microsec_wait_idle_thread_delay));
  _read_from_stream_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_read_from_stream_timer_handler(err);
      });
}

/**
 * @brief read events from _client and write to _muxer
 *
 * @param err
 */
void feeder::_read_from_stream_timer_handler(
    const boost::system::error_code& err) {
  if (err) {
    return;
  }
  std::deque<std::shared_ptr<io::data>> events_to_publish;
  std::shared_ptr<io::data> event;
  std::chrono::system_clock::time_point timeout_read =
      std::chrono::system_clock::now() + std::chrono::milliseconds(100);
  try {
    std::unique_lock<std::timed_mutex> l(_protect);
    if (_state != state::running)
      return;
    while (std::chrono::system_clock::now() < timeout_read &&
           events_to_publish.size() < max_event_queue_size) {
      if (!_client->read(event, 0)) {  // nothing to read
        break;
      }
      if (event) {  // event is null if not decoded by bbdo stream
        if (log_v2::processing()->level() == spdlog::level::trace)
          SPDLOG_LOGGER_TRACE(
              log_v2::processing(),
              "feeder '{}': sending 1 event {} from stream to muxer", _name,
              *event);
        else
          SPDLOG_LOGGER_DEBUG(
              log_v2::processing(),
              "feeder '{}': sending 1 event {} from stream to muxer", _name,
              event->type());

        events_to_publish.push_back(event);
      }
    }
  } catch (exceptions::shutdown const&) {
    // Normal termination.
    SPDLOG_LOGGER_INFO(log_v2::core(), "from client feeder '{}' shutdown",
                       _name);
    _muxer->write(events_to_publish);
    // _client->read shutdown => we stop read and don't restart the read from
    // stream timer
    return;
  } catch (const exceptions::connection_closed&) {
    set_last_error("");
    SPDLOG_LOGGER_DEBUG(log_v2::processing(), "feeder '{}', connection closed",
                        _name);
    _muxer->write(events_to_publish);
    stop();
    return;
  } catch (const std::exception& e) {
    set_last_error(e.what());
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "from client feeder '{}' error:{} ", _name, e.what());
    _muxer->write(events_to_publish);
    stop();
    return;
  } catch (...) {
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "from client feeder: unknown error occured while "
                        "processing client '{}'",
                        _name);
    _muxer->write(events_to_publish);
    stop();
    return;
  }
  _muxer->write(events_to_publish);
  _start_read_from_stream_timer();
}
