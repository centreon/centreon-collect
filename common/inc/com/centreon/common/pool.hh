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
#ifndef CENTREON_COMMON_POOL_HH
#define CENTREON_COMMON_POOL_HH

#include <forward_list>
namespace com::centreon::common {

/**
 * @brief The Broker's thread pool.
 *
 * At the origin, this thread pool is configured to be used by ASIO. Each thread
 * in this pool runs an io_context that allows any part in Broker to post
 * a work to be done.
 *
 * This pool may look a little complicated. We can see inside it several
 * attributes, let's make a quick tour of them. A thread pool is an array of
 * threads. This array, here, is a std::forward_list named _pool.
 *
 * This pool is instanciated through a unique instance. So its constructor is
 * private and we have a static method named instance() to get it.
 *
 * Initially, the pool is not started. To start it, there is a static method
 * named set_pool_size() that takes one argument: the number of threads.
 * This method can be called several times, if pool_size is greater than
 * previous one, pool_size is increased, otherwise it has no effect
 *
 * So we call first load in main of cbd or engine and then we have to call
 * set_pool_size to start thread(s)
 *
 * In order to avoid link issues, _instance member must be defined in final
 * executables
 */
class pool {
  static std::unique_ptr<pool> _instance;

  const std::shared_ptr<asio::io_context> _io_context;
  const std::shared_ptr<spdlog::logger> _logger;
  asio::executor_work_guard<asio::io_context::executor_type> _worker;
  size_t _pool_size;
  std::forward_list<std::thread>* _pool ABSL_GUARDED_BY(_pool_m);
  pid_t _original_pid;
  mutable absl::Mutex _pool_m;

  void _stop() ABSL_LOCKS_EXCLUDED(_pool_m);
  void _set_pool_size(size_t pool_size) ABSL_LOCKS_EXCLUDED(_pool_m);

 public:
  pool(const std::shared_ptr<asio::io_context>& io_context,
       const std::shared_ptr<spdlog::logger>& logger);
  pool(const pool&) = delete;
  pool& operator=(const pool&) = delete;
  ~pool();

  static void load(const std::shared_ptr<asio::io_context>& io_context,
                   const std::shared_ptr<spdlog::logger>& logger);
  static void unload();
  static pool& instance();
  static asio::io_context& io_context();
  static std::shared_ptr<asio::io_context> io_context_ptr();
  static void set_pool_size(size_t pool_size);

  /**
   * @brief Returns the number of threads used in the pool.
   *
   * @return a size.
   */
  size_t get_pool_size() const { return _pool_size; }
};

}  // namespace com::centreon::common

#endif  // CENTREON_COMMON_POOL_HH
