/**
 * Copyright 2011-2012,2015,2017, 2020-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/broker/processing/feeder.hh"

#include <unistd.h>

#include "com/centreon/broker/exceptions/connection_closed.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::processing;
using log_v2 = com::centreon::common::log_v2::log_v2;

constexpr unsigned max_event_queue_size = 0x10000;

/**
 *  feeder is an enable_shared_from_this, this static method ensures that object
 * is created by a new
 *
 *  @param[in] name           Name.
 *  @param[in] parent         Engine relying muxers.
 *  @param[in] client         Client stream.
 *  @param[in] read_filters   Read filters.
 *  @param[in] write_filters  Write filters.
 */
std::shared_ptr<feeder> feeder::create(
    const std::string& name,
    const std::shared_ptr<multiplexing::engine>& parent,
    std::shared_ptr<io::stream>& client,
    const multiplexing::muxer_filter& read_filters,
    const multiplexing::muxer_filter& write_filters) {
  std::shared_ptr<feeder> ret(
      new feeder(name, parent, client, read_filters, write_filters));

  ret->init();
  return ret;
}

/**
 * @brief to call after object construction
 *
 */
void feeder::init() {
  _start_stat_timer();
  _muxer->set_action_on_new_data(shared_from_this());

  _start_read_from_stream_timer();
}

/**
 *  Constructor.
 *
 *  @param[in] name           Name.
 *  @param[in] parent         Engine relying muxers.
 *  @param[in] client         Client stream.
 *  @param[in] read_filters   Read filters.
 *  @param[in] write_filters  Write filters.
 */
feeder::feeder(const std::string& name,
               const std::shared_ptr<multiplexing::engine>& parent,
               std::shared_ptr<io::stream>& client,
               const multiplexing::muxer_filter& read_filters,
               const multiplexing::muxer_filter& write_filters)
    : stat_visitable(name),
      _state{state::running},
      _client(std::move(client)),
      _muxer(multiplexing::muxer::create(name,
                                         parent,
                                         std::move(read_filters),
                                         std::move(write_filters),
                                         false)),
      _stat_timer(com::centreon::common::pool::io_context()),
      _read_from_stream_timer(com::centreon::common::pool::io_context()),
      _io_context(com::centreon::common::pool::io_context_ptr()),
      _logger{log_v2::instance().get(log_v2::PROCESSING)} {
  if (!_client)
    throw msg_fmt("could not process '{}' with no client stream", _name);

  set_last_connection_attempt(timestamp::now());
  set_last_connection_success(timestamp::now());
  set_state("connected");
  SPDLOG_LOGGER_DEBUG(_logger, "create feeder {}, {:p}", name,
                      static_cast<const void*>(this));
}

/**
 *  Destructor.
 */
feeder::~feeder() {
  stop();
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

/**
 * @brief write event to client stream
 * @param event
 * @return number of events written
 */
uint32_t feeder::on_events(
    const std::vector<std::shared_ptr<io::data>>& events) {
  unsigned written = 0;
  try {
    for (const std::shared_ptr<io::data>& event : events) {
      if (_logger->level() == spdlog::level::trace) {
        SPDLOG_LOGGER_TRACE(
            _logger,
            "feeder '{}': sending 1 event {:x} from muxer to stream {}", _name,
            event->type(), *event);
      } else {
        SPDLOG_LOGGER_DEBUG(
            _logger, "feeder '{}': sending 1 event {:x} from muxer to stream",
            _name, event->type());
      }
      _client->write(event);
      ++written;
    }
  } catch (exceptions::shutdown const&) {
    // Normal termination.
    SPDLOG_LOGGER_INFO(_logger, "from muxer feeder '{}' shutdown", _name);
  } catch (const exceptions::connection_closed&) {
    set_last_error("");
    SPDLOG_LOGGER_INFO(_logger, "feeder '{}' connection closed", _name);
  } catch (const std::exception& e) {
    set_last_error(e.what());
    SPDLOG_LOGGER_ERROR(_logger, "from muxer feeder '{}' error:{} ", _name,
                        e.what());
  } catch (...) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "from muxer feeder: unknown error occured while "
                        "processing client '{}'",
                        _name);
  }
  return written;
}

/**
 * @brief acknowledge events to the muxer and catch exception from it
 *
 * @param count
 */
void feeder::_ack_events_on_muxer(unsigned count) noexcept {
  try {
    _muxer->ack_events(count);
  } catch (const std::exception&) {
    SPDLOG_LOGGER_ERROR(_logger, " {} fail to acknowledge {} events", _name,
                        count);
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
  SPDLOG_LOGGER_INFO(_logger, "{} Stop without lock called", _name);
  state expected = state::running;
  if (!_state.compare_exchange_strong(expected, state::finished)) {
    return;
  }

  set_state("disconnected");

  _muxer->clear_action_on_new_data();
  // muxer should not receive events
  _muxer->unsubscribe();
  _stat_timer.cancel();
  _read_from_stream_timer.cancel();

  /* We don't get back the return value of stop() because it has non sense,
   * the only interest in calling stop() is to send an acknowledgement to the
   * peer. */
  try {
    _client->stop();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "{} Failed to send stop event to client: {}",
                        _name, e.what());
  }
  SPDLOG_LOGGER_INFO(_logger, "feeder: queue files of client '{}' removed",
                     _name);
  _muxer->remove_queue_files();
  SPDLOG_LOGGER_INFO(_logger, "feeder: {} terminated", _name);
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
  _stat_timer.expires_after(std::chrono::seconds(5));
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
  _read_from_stream_timer.expires_after(
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
        if (_logger->level() == spdlog::level::trace)
          SPDLOG_LOGGER_TRACE(
              _logger, "feeder '{}': sending 1 event {} from stream to muxer",
              _name, *event);
        else
          SPDLOG_LOGGER_DEBUG(
              _logger, "feeder '{}': sending 1 event {} from stream to muxer",
              _name, event->type());

        events_to_publish.push_back(event);
      }
    }
  } catch (exceptions::shutdown const&) {
    // Normal termination.
    SPDLOG_LOGGER_INFO(_logger, "from client feeder '{}' shutdown", _name);
    _muxer->write(events_to_publish);
    // _client->read shutdown => we stop read and don't restart the read from
    // stream timer
    return;
  } catch (const exceptions::connection_closed&) {
    set_last_error("");
    SPDLOG_LOGGER_INFO(_logger, "feeder '{}', connection closed", _name);
    _muxer->write(events_to_publish);
    stop();
    return;
  } catch (const std::exception& e) {
    set_last_error(e.what());
    SPDLOG_LOGGER_ERROR(_logger, "from client feeder '{}' error:{} ", _name,
                        e.what());
    _muxer->write(events_to_publish);
    stop();
    return;
  } catch (...) {
    SPDLOG_LOGGER_ERROR(_logger,
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
