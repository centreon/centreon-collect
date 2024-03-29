/**
 * Copyright 2020-2024 Centreon
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

#include <spdlog/spdlog.h>
#include <forward_list>
#include <mutex>
#include <thread>

#include "com/centreon/common/pool.hh"

namespace asio = boost::asio;
using namespace com::centreon::common;

std::unique_ptr<pool> pool::_instance;

/**
 * @brief The way to access to the pool.
 *
 * @return a reference to the pool.
 */
pool& com::centreon::common::pool::instance() {
  assert(pool::_instance);
  return *_instance;
}

/**
 * @brief create singleton
 * must be called in main only
 *
 * @param io_context
 * @param logger
 */
void pool::load(const std::shared_ptr<asio::io_context>& io_context,
                const std::shared_ptr<spdlog::logger>& logger) {
  if (_instance == nullptr)
    _instance = std::make_unique<pool>(io_context, logger);
  else
    SPDLOG_LOGGER_ERROR(logger, "pool already started.");
}

/**
 * @brief unload singleton
 * must be called in main only
 *
 * @param io_context
 * @param logger
 */
void pool::unload() {
  _instance.reset();
}

/**
 * @brief A static method to access the IO context.
 *
 * @return the IO context.
 */
asio::io_context& com::centreon::common::pool::io_context() {
  return *instance()._io_context;
}

/**
 * @brief A static method to access the IO context.
 *
 * @return the IO context.
 */
std::shared_ptr<asio::io_context> pool::io_context_ptr() {
  return instance()._io_context;
}

/**
 * @brief Constructor.
 * it doesn't start any thread, this is the job of set_pool_size
 * Don't use it, use pool::load instead
 */
pool::pool(const std::shared_ptr<asio::io_context>& io_context,
           const std::shared_ptr<spdlog::logger>& logger)
    : _io_context(io_context),
      _logger(logger),
      _worker{asio::make_work_guard(*_io_context)},
      _pool_size(0) {}

/**
 * @brief Destructor
 */
pool::~pool() {
  _stop();
}

/**
 * @brief Stop the thread pool.
 */
void pool::_stop() {
  SPDLOG_LOGGER_DEBUG(_logger, "Stopping the thread pool");
  std::lock_guard<std::mutex> lock(_pool_m);
  _worker.reset();
  for (auto& t : _pool)
    if (t.joinable())
      t.join();
  _pool.clear();
  SPDLOG_LOGGER_DEBUG(_logger, "No remaining thread in the pool");
}

/**
 * @brief
 *
 * @param pool_size
 */
void pool::_set_pool_size(size_t pool_size) {
  size_t new_size = pool_size == 0
                        ? std::max(std::thread::hardware_concurrency(), 3u)
                        : pool_size;

  std::lock_guard<std::mutex> lock(_pool_m);
  SPDLOG_LOGGER_INFO(_logger, "Starting the thread pool with {} thread{}",
                     new_size, new_size > 1 ? "s" : "");

  for (; _pool_size < new_size; ++_pool_size) {
    auto& new_thread =
        _pool.emplace_front([ctx = _io_context, logger = _logger] {
          try {
            SPDLOG_LOGGER_DEBUG(logger, "start of asio thread {:x}",
                                pthread_self());
            ctx->run();
          } catch (const std::exception& e) {
            SPDLOG_LOGGER_CRITICAL(logger,
                                   "catch in io_context run: {} {} thread {:x}",
                                   e.what(), typeid(e).name(), pthread_self());
          }
        });

    pthread_setname_np(new_thread.native_handle(),
                       fmt::format("pool_thread{}", _pool_size).c_str());
  }
}

/**
 * @brief set pool size
 * it only increases pool size
 *
 * @param pool_size if 0 it set size to max(nb core, 3)
 */
void pool::set_pool_size(size_t pool_size) {
  if (_instance) {
    _instance->_set_pool_size(pool_size);
  }
}
