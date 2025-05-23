/**
 * Copyright 2011-2021 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/logging/broker.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/unique_array_ptr.hh"

using namespace com::centreon;
using namespace com::centreon::engine::logging;

/**
 *  Default constructor.
 */
engine::logging::broker::broker()
    : backend(false, false, com::centreon::logging::none, false),
      _enable(false),
      _thread_id{} {
  open();
}

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
engine::logging::broker::broker(broker const& right)
    : backend(right), _enable(false), _thread_id{right._thread_id} {}

/**
 *  Destructor.
 */
engine::logging::broker::~broker() noexcept {
  close();
}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
engine::logging::broker& engine::logging::broker::operator=(
    broker const& right) {
  if (this != &right) {
    backend::operator=(right);
    std::lock_guard<std::recursive_mutex> lock1(_lock);
    std::lock_guard<std::recursive_mutex> lock2(right._lock);
    _enable = right._enable;
    _thread_id = right._thread_id;
  }
  return *this;
}

/**
 *  Close broker log.
 */
void engine::logging::broker::close() noexcept {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  _enable = false;
}

/**
 *  Send message to broker.
 *
 *  @param[in] type     Logging types.
 *  @param[in] verbose  Verbosity level.
 *  @param[in] message  Message to log.
 *  @param[in] size     Message length.
 */
void engine::logging::broker::log(uint64_t types [[maybe_unused]],
                                  uint32_t verbose,
                                  char const* message,
                                  uint32_t size) noexcept {
  (void)verbose;
  std::lock_guard<std::recursive_mutex> lock(_lock);

  if (_thread_id != std::this_thread::get_id()) {
    // Broker is only notified of non-debug log messages.
    if (message && _enable) {
      _thread_id = std::this_thread::get_id();
      // Copy message because broker module might modify it.
      unique_array_ptr<char> copy(new char[size + 1]);
      strncpy(copy.get(), message, size);
      copy.get()[size] = 0;

      // Event broker callback.
      broker_log_data_legacy(copy.get(), time(NULL));
      _thread_id = std::thread::id();
    }
  }
}

/**
 *  Open broker log.
 */
void engine::logging::broker::open() {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  _enable = true;
}

/**
 *  Open borker log.
 */
void engine::logging::broker::reopen() {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  _enable = true;
}
