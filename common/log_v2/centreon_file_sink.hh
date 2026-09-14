/**
 * Copyright 2024-2026 Centreon
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
#pragma once

#include <spdlog/common.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/os.h>
#include <spdlog/details/synchronous_factory.h>
#include <spdlog/sinks/base_sink.h>

namespace spdlog {
namespace sinks {

class centreon_file_sink_base {
 public:
  virtual ~centreon_file_sink_base() = default;
  virtual bool set_filename(const std::string& new_filename) = 0;
  virtual filename_t filename() = 0;
};

/*
 * Trivial file sink with single file as target
 */
template <typename Mutex>
class centreon_file_sink final : public base_sink<Mutex>,
                                 public centreon_file_sink_base {
 public:
  explicit centreon_file_sink(const filename_t& filename,
                              bool truncate = false,
                              const file_event_handlers& event_handlers = {});
  filename_t filename() override;
  bool set_filename(const std::string& new_filename) override;
  void reopen();

 protected:
  void sink_it_(const details::log_msg& msg) override;
  void flush_() override;

 private:
  details::file_helper file_helper_;
};

using centreon_file_sink_mt = centreon_file_sink<std::mutex>;
using centreon_file_sink_st = centreon_file_sink<details::null_mutex>;

template <typename Mutex>
SPDLOG_INLINE centreon_file_sink<Mutex>::centreon_file_sink(
    const filename_t& filename,
    bool truncate,
    const file_event_handlers& event_handlers)
    : file_helper_{event_handlers} {
  file_helper_.open(filename, truncate);
}

template <typename Mutex>
SPDLOG_INLINE filename_t centreon_file_sink<Mutex>::filename() {
  std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
  return file_helper_.filename();
}

/**
 * @brief Reopen the sink on new_filename, but only if it differs from the
 * current target, so unrelated config reloads don't force a needless file
 * reopen.
 *
 * @param new_filename The new file to log into.
 *
 * @return true if the filename changed and the file was reopened, false
 * otherwise.
 */
template <typename Mutex>
SPDLOG_INLINE bool centreon_file_sink<Mutex>::set_filename(
    const std::string& new_filename) {
  std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
  if (new_filename != file_helper_.filename()) {
    file_helper_.open(new_filename, false);
    return true;
  } else {
    return false;
  }
}

template <typename Mutex>
SPDLOG_INLINE void centreon_file_sink<Mutex>::reopen() {
  std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
  file_helper_.reopen(false);
}

template <typename Mutex>
SPDLOG_INLINE void centreon_file_sink<Mutex>::sink_it_(
    const details::log_msg& msg) {
  memory_buf_t formatted;
  base_sink<Mutex>::formatter_->format(msg, formatted);
  file_helper_.write(formatted);
}

template <typename Mutex>
SPDLOG_INLINE void centreon_file_sink<Mutex>::flush_() {
  file_helper_.flush();
}
}  // namespace sinks

//
// factory functions
//
template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> basic_logger_mt(
    const std::string& logger_name,
    const filename_t& filename,
    bool truncate = false,
    const file_event_handlers& event_handlers = {}) {
  return Factory::template create<sinks::centreon_file_sink_mt>(
      logger_name, filename, truncate, event_handlers);
}

template <typename Factory = spdlog::synchronous_factory>
inline std::shared_ptr<logger> basic_logger_st(
    const std::string& logger_name,
    const filename_t& filename,
    bool truncate = false,
    const file_event_handlers& event_handlers = {}) {
  return Factory::template create<sinks::centreon_file_sink_st>(
      logger_name, filename, truncate, event_handlers);
}

}  // namespace spdlog
