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
  ret->_start_stat_timer<true>();

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
}

/**
 *  Destructor.
 */
feeder::~feeder() {
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
  if (_client) {
    _client->statistics(tree);
  }
  _muxer->statistics(tree);
}

/**
 * @brief read event from muxer (both synchronous or asynchronous)
 * if an event is available => call _on_event_from_muxer
 * if not, _on_event_from_muxer will be called from muxer when event arrive
 */
void feeder::_read_from_muxer() {
  if (_state != state::running) {
    return;
  }
  bool have_to_terminate = false;
  std::shared_ptr<io::data> event;
  try {
    std::shared_ptr<multiplexing::muxer> mux;
    {
      std::unique_lock<std::mutex> l(_protect);
      if (!_muxer) {
        return;
      }
      mux = _muxer;
    }
    event = mux->read([me = shared_from_this()]() { me->_read_from_muxer(); });
  } catch (exceptions::shutdown const&) {
    // Normal termination.
    SPDLOG_LOGGER_INFO(log_v2::core(), "from muxer feeder '{}' shutdown",
                       _name);
    have_to_terminate = true;
  } catch (const std::exception& e) {
    set_last_error(e.what());
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "from muxer feeder '{}' error:{} ", _name, e.what());
    have_to_terminate = true;
  } catch (...) {
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "from muxer feeder: unknown error occured while "
                        "processing client '{}'",
                        _name);
    have_to_terminate = true;
  }
  if (event) {
    _on_event_from_muxer(event);
  }
  if (have_to_terminate) {
    stop();
  }
}

/**
 * @brief called when muxer event is available
 * write event to client stream
 * @param event
 */
void feeder::_on_event_from_muxer(const std::shared_ptr<io::data>& event) {
  if (log_v2::processing()->level() == spdlog::level::trace)
    SPDLOG_LOGGER_TRACE(log_v2::processing(),
                        "feeder '{}': sending 1 event {} from muxer to stream",
                        _name, *event);
  else
    SPDLOG_LOGGER_DEBUG(
        log_v2::processing(),
        "feeder '{}': sending 1 event {:x} from muxer to stream", _name,
        event->type());
  bool have_to_terminate = false;
  try {
    std::shared_ptr<io::stream> client;
    std::shared_ptr<multiplexing::muxer> mux;
    {
      std::unique_lock<std::mutex> l(_protect);
      if (!_client) {
        return;
      }
      client = _client;
      mux = _muxer;
    }
    client->write(event);
    mux->ack_events(1);
  } catch (exceptions::shutdown const&) {
    // Normal termination.
    SPDLOG_LOGGER_INFO(log_v2::core(), "from muxer feeder '{}' shutdown",
                       _name);
    have_to_terminate = true;
  } catch (const std::exception& e) {
    set_last_error(e.what());
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "from muxer feeder '{}' error:{} ", _name, e.what());
    have_to_terminate = true;
  } catch (...) {
    SPDLOG_LOGGER_ERROR(
        log_v2::processing(),
        "from muxer feeder: unknown error occured while processing client '{}'",
        _name);
    have_to_terminate = true;
  }
  if (have_to_terminate) {
    stop();
  } else {  // another event to read?
    _read_from_muxer();
  }
}

void feeder::stop() {
  state expected = state::running;
  if (!_state.compare_exchange_strong(expected, state::finished)) {
    return;
  }

  set_state("disconnected");

  std::unique_lock<std::mutex> l(_protect);
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
                        "Failed to send stop event to client: {}", e.what());
  }
  _client.reset();
  SPDLOG_LOGGER_INFO(log_v2::core(),
                     "feeder: queue files of client '{}' removed", _name);
  _muxer->remove_queue_files();
  SPDLOG_LOGGER_INFO(log_v2::core(), "feeder: {} terminated", _name);
  // in order to avoid circular owning
  _muxer.reset();
}

uint32_t feeder::_get_queued_events() const {
  return _muxer->get_event_queue_size();
}

bool feeder::wait_for_all_events_written(unsigned ms_timeout) {
  std::unique_lock<std::mutex> l(_protect);
  if (_client) {
    return _client->wait_for_all_events_written(ms_timeout);
  }
  return true;
}

template <bool to_lock>
void feeder::_start_stat_timer() {
  std::unique_lock<std::mutex> l(_protect, std::defer_lock);
  if (to_lock) {
    l.lock();
  }
  _stat_timer.expires_from_now(std::chrono::seconds(5));
  _stat_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_stat_timer_handler(err);
      });
}

void feeder::_stat_timer_handler(const boost::system::error_code& err) {
  if (err) {
    return;
  }
  std::unique_lock<std::mutex> l(_protect);
  if (!_muxer) {
    return;
  }
  set_queued_events(_muxer->get_event_queue_size());
  _start_stat_timer<false>();
}

void feeder::_start_read_from_stream_timer() {
  std::unique_lock<std::mutex> l(_protect);
  _read_from_stream_timer.expires_from_now(
      std::chrono::microseconds(idle_microsec_wait_idle_thread_delay));
  _read_from_stream_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        me->_read_from_stream_timer_handler(err);
      });
}

void feeder::_read_from_stream_timer_handler(
    const boost::system::error_code& err) {
  if (err) {
    return;
  }
  std::shared_ptr<multiplexing::muxer> mux;
  std::shared_ptr<io::stream> client;
  {
    std::unique_lock<std::mutex> l(_protect);
    if (!_muxer) {
      return;
    }
    mux = _muxer;
    if (!_client) {
      return;
    }
    client = _client;
  }
  std::list<std::shared_ptr<io::data>> events_to_publish;
  std::shared_ptr<io::data> event;
  try {
    std::chrono::system_clock::time_point timeout_read =
        std::chrono::system_clock::now() + std::chrono::milliseconds(100);
    while (client->read(event, 0) &&
           std::chrono::system_clock::now() < timeout_read) {
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
    mux->write(events_to_publish);
  } catch (exceptions::shutdown const&) {
    // Normal termination.
    SPDLOG_LOGGER_INFO(log_v2::core(), "from client feeder '{}' shutdown",
                       _name);
    stop();
    return;
  } catch (const std::exception& e) {
    set_last_error(e.what());
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "from client feeder '{}' error:{} ", _name, e.what());
    stop();
    return;
  } catch (...) {
    SPDLOG_LOGGER_ERROR(log_v2::processing(),
                        "from client feeder: unknown error occured while "
                        "processing client '{}'",
                        _name);
    stop();
    return;
  }
  _start_read_from_stream_timer();
}
