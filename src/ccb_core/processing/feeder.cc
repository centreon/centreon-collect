/*
** Copyright 2011-2012,2015,2017 Centreon
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
#include <cassert>
#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::processing;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Constructor.
 *
 *  @param[in] name           Name.
 *  @param[in] client         Client stream.
 *  @param[in] read_filters   Read filters.
 *  @param[in] write_filters  Write filters.
 */
feeder::feeder(std::string const& name,
               std::shared_ptr<io::stream> client,
               std::unordered_set<uint32_t> const& read_filters,
               std::unordered_set<uint32_t> const& write_filters)
    : stat_visitable(name),
      _started{false},
      _stopped{false},
      _should_exit{false},
      _client(client),
      _subscriber(name, false) {
  _subscriber.get_muxer().set_read_filters(read_filters);
  _subscriber.get_muxer().set_write_filters(write_filters);
  // By default, we assume the feeder is already connected.
  set_last_connection_attempt(timestamp::now());
  set_last_connection_success(timestamp::now());
  set_state("connecting");
}

/**
 *  Destructor.
 */
feeder::~feeder() { exit(); }

void feeder::exit() {
  std::unique_lock<std::mutex> lock(_stopped_m);
  if (_started) {
    if (!_should_exit) {
      _should_exit = true;
      _stopped_cv.wait(lock, [this] { return _stopped; });
      _thread.join();
      _started = false;
    }
  }
}

bool feeder::is_finished() const noexcept {
  std::lock_guard<std::mutex> lock(_started_m);
  return !_started && _should_exit;
}

/**
 *  Get the read filters used by the feeder.
 *
 *  @return  The read filters used by the feeder.
 */
std::string const& feeder::_get_read_filters() const {
  return _subscriber.get_muxer().get_read_filters_str();
}

/**
 *  Get the write filters used by the feeder.
 *
 *  @return  The write filters used by the feeder.
 */
std::string const& feeder::_get_write_filters() const {
  return _subscriber.get_muxer().get_write_filters_str();
}

/**
 *  Forward to stream.
 *
 *  @param[in] tree  The statistic tree.
 */
void feeder::_forward_statistic(json11::Json::object& tree) {
  if (_client_m.try_lock_shared_for(300)) {
    if (_client)
      _client->statistics(tree);
    _client_m.unlock();
  }
  _subscriber.get_muxer().statistics(tree);
}

void feeder::start() {
  std::unique_lock<std::mutex> lock(_started_m);
  _stopped = false;
  if (!_client)
      throw msg_fmt("could not process '{}' with no client stream", _name);
  if (!_started) {
    _should_exit = false;
    _thread = std::thread(&feeder::_callback, this);
    _started_cv.wait(lock, [this] { return _started; });
  }
}

void feeder::_callback() noexcept {
  assert(_client);
  logging::info(logging::medium) << "feeder: thread of client '" << _name
                                 << "' is starting";
  time_t fill_stats_time = time(nullptr);
  std::unique_lock<std::mutex> lock(_started_m);

  try {
    set_state("connected");
    bool stream_can_read(true);
    bool muxer_can_read(true);
    std::shared_ptr<io::data> d;
    _started = true;
    _started_cv.notify_all();
    lock.unlock();
    while (!_should_exit) {
      // Read from stream.
      bool timed_out_stream(true);

      // Filling stats
      if (time(nullptr) >= fill_stats_time) {
        fill_stats_time += 5;
        set_queued_events(_subscriber.get_muxer().get_event_queue_size());
      }

      if (stream_can_read) {
        try {
          misc::read_lock lock(_client_m);
          timed_out_stream = !_client->read(d, 0);
        }
        catch (shutdown const& e) {
          stream_can_read = false;
        }
        if (d) {
          {
            misc::read_lock lock(_client_m);
            _subscriber.get_muxer().write(d);
          }
          tick();
          continue;  // Stream read bias.
        }
      }

      // Read from muxer.
      d.reset();
      bool timed_out_muxer(true);
      if (muxer_can_read)
        try {
          timed_out_muxer = !_subscriber.get_muxer().read(d, 0);
        }
      catch (shutdown const& e) {
        muxer_can_read = false;
      }
      if (d) {
        {
          misc::read_lock lock(_client_m);
          _client->write(d);
        }
        _subscriber.get_muxer().ack_events(1);
        tick();
      }

      // If both timed out, sleep a while.
      d.reset();
      if (timed_out_stream && timed_out_muxer)
        ::usleep(100000);
    }
  }
  catch (shutdown const& e) {
    // Normal termination.
    (void)e;
  }
  catch (std::exception const& e) {
    logging::error(logging::medium)
        << "feeder: error occured while processing client '" << _name
        << "': " << e.what();
    set_last_error(e.what());
  }
  catch (...) {
    logging::error(logging::high)
        << "feeder: unknown error occured while processing client '" << _name
        << "'";
  }

  std::unique_lock<std::mutex> lock_stop(_stopped_m);
  _stopped = true;
  _stopped_cv.notify_all();
  lock_stop.unlock();

  {
    misc::read_lock lock(_client_m);
    _client.reset();
    set_state("disconnected");
    _subscriber.get_muxer().remove_queue_files();
  }
  logging::info(logging::medium) << "feeder: thread of client '" << _name
                                 << "' will exit";
}
