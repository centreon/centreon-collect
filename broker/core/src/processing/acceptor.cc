/*
** Copyright 2015-2022 Centreon
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

#include "com/centreon/broker/processing/acceptor.hh"

#include <unistd.h>

#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/processing/feeder.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::processing;

/**
 *  Constructor.
 *
 *  @param[in] endp       Endpoint.
 *  @param[in] name       Name of the endpoint.
 */
acceptor::acceptor(std::shared_ptr<io::endpoint> endp,
                   std::string const& name,
                   const multiplexing::muxer_filter& r_filter,
                   const multiplexing::muxer_filter& w_filter)
    : endpoint(true, name),
      _state(stopped),
      _should_exit(false),
      _endp(endp),
      _read_filters(r_filter),
      _read_filters_str(misc::dump_filters(_read_filters)),
      _write_filters(w_filter),
      _write_filters_str(misc::dump_filters(_write_filters)) {
  log_v2::config()->trace(
      "processing::acceptor '{}': read filter <<{}>> ; write filter <<{}>>",
      name, _read_filters_str, _write_filters_str);
}

/**
 *  Destructor.
 */
acceptor::~acceptor() {
  exit();
}

/**
 *  Accept a new incoming connection.
 */
void acceptor::accept() {
  static std::atomic_uint connection_id{0};

  // Try to accept connection.
  std::shared_ptr<io::stream> u = _endp->open();

  if (u) {
    // Create feeder thread.
    std::string name(fmt::format("{}-{}", _name, ++connection_id));
    SPDLOG_LOGGER_INFO(log_v2::core(), "New incoming connection '{}'", name);
    log_v2::config()->debug(
        "New feeder {} with read_filters {} and write_filters {}", name,
        _read_filters.get_allowed_categories(),
        _write_filters.get_allowed_categories());
    std::shared_ptr<feeder> f =
        feeder::create(name, multiplexing::engine::instance_ptr(), u,
                       _read_filters, _write_filters);

    std::lock_guard<std::mutex> lock(_stat_mutex);
    _feeders.push_back(f);
    SPDLOG_LOGGER_TRACE(log_v2::core(),
                        "Currently {} connections to acceptor '{}'",
                        _feeders.size(), _name);
  } else
    log_v2::core()->debug("accept ('{}') failed.", _name);
}

/**
 *  Exit this thread.
 */
void acceptor::exit() {
  std::unique_lock<std::mutex> lck(_state_m);
  switch (_state) {
    case stopped:
      _state = finished;
      break;
    case running:
      _should_exit = true;
      _state_cv.wait(lck, [this] { return _state == acceptor::finished; });
      break;
    case finished:
      break;
  }

  lck.unlock();
  if (_thread && _thread->joinable()) {
    _thread->join();
  }

  for (auto& feeder : _feeders) {
    feeder->stop();
  }
  _feeders.clear();
}

/**
 *  @brief Set retry interval of the acceptor.
 *
 *  The retry interval is only used in case of error of the acceptor. In
 *  normal operation mode, connections are accepted as soon as possible.
 *
 *  @param[in] retry_interval  Retry interval between two client
 *                             acception attempts in case of error at
 *                             the first attempt.
 */
void acceptor::set_retry_interval(time_t retry_interval) {
  _retry_interval = retry_interval;
}

/**
 *  Get the read filters used by the feeder.
 *
 *  @return  The read filters used by the feeder.
 */
std::string const& acceptor::_get_read_filters() const {
  return _read_filters_str;
}

/**
 *  Get the write filters used by the feeder.
 *
 *  @return  The write filters used by the feeder.
 */
std::string const& acceptor::_get_write_filters() const {
  return _write_filters_str;
}

/**
 *  Forward the statistic to the feeders.
 *
 *  @param[in] tree  The tree.
 */
void acceptor::_forward_statistic(nlohmann::json& tree) {
  // Get statistic of acceptor.
  _endp->stats(tree);
  // Get statistics of feeders
  for (auto it = _feeders.begin(), end = _feeders.end(); it != end; ++it) {
    nlohmann::json subtree;
    (*it)->stats(subtree);
    tree[(*it)->get_name()] = std::move(subtree);
  }
}

void acceptor::_set_listening(bool listening) noexcept {
  _listening = listening;
  set_state(listening ? "listening" : "disconnected");
}

uint32_t acceptor::_get_queued_events() const {
  return 0;
}

/**
 *  Start bthread.
 */
void acceptor::start() {
  std::unique_lock<std::mutex> lock(_state_m);
  if (_state == stopped) {
    _should_exit = false;
    _thread = std::make_unique<std::thread>(&acceptor::_callback, this);
    _state_cv.wait(lock, [this] { return _state == acceptor::running; });
    pthread_setname_np(_thread->native_handle(), "proc_acceptor");
  }
}

void acceptor::_callback() noexcept {
  std::unique_lock<std::mutex> lock(_state_m);
  _state = running;
  _state_cv.notify_all();
  lock.unlock();

  // Run as long as no exit request was made.
  while (!_should_exit) {
    try {
      _set_listening(true);
      // Try to accept connection.
      accept();
    } catch (std::exception const& e) {
      _set_listening(false);
      // Log error.
      SPDLOG_LOGGER_ERROR(log_v2::core(),
                          "acceptor: endpoint '{}' could not accept client: {}",
                          _name, e.what());

      // Sleep a while before reconnection.
      log_v2::core()->debug(
          "acceptor: endpoint '{}' will wait {}s before attempting to accept a "
          "new client",
          _name, _retry_interval);
      time_t limit{time(nullptr) + _retry_interval};
      while (!_endp->is_ready() && !_should_exit && time(nullptr) < limit) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }

    // Check for terminated feeders.
    {
      std::lock_guard<std::mutex> lock(_stat_mutex);
      for (auto it = _feeders.begin(), end = _feeders.end(); it != end;) {
        SPDLOG_LOGGER_TRACE(log_v2::core(), "acceptor '{}' feeder '{}'", _name,
                            (*it)->get_name());
        if ((*it)->is_finished()) {
          SPDLOG_LOGGER_INFO(log_v2::core(), "removing '{}' from acceptor '{}'",
                             (*it)->get_name(), _name);
          it = _feeders.erase(it);
        } else
          ++it;
      }
    }
  }
  SPDLOG_LOGGER_INFO(log_v2::core(), "processing acceptor '{}' finished",
                     _name);
  _set_listening(false);

  lock.lock();
  _state = acceptor::finished;
  _state_cv.notify_all();
}

/**
 * @brief
 *
 * @param ms_timeout
 * @return true
 * @return false
 */
bool acceptor::wait_for_all_events_written(unsigned ms_timeout) {
  std::lock_guard<std::mutex> lock(_stat_mutex);
  bool ret = true;
  for (std::shared_ptr<feeder> to_wait : _feeders) {
    ret &= to_wait->wait_for_all_events_written(ms_timeout);
  }
  return ret;
}
