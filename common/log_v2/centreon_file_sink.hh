/**
 * Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 *
 * This file is copied from basic_file_sink{-inl.h,.h}
 * The goal here is just to add a method `reopen()` using the file_helper mutex.
 */
#pragma once

#include <spdlog/common.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/os.h>
#include <spdlog/details/synchronous_factory.h>
#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>

namespace spdlog {
namespace sinks {
/*
 * Trivial file sink with single file as target
 */
template <typename Mutex>
class centreon_file_sink final : public base_sink<Mutex> {
 public:
  explicit centreon_file_sink(const filename_t& filename,
                              bool truncate = false,
                              const file_event_handlers& event_handlers = {});
  const filename_t& filename() const;
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
SPDLOG_INLINE const filename_t& centreon_file_sink<Mutex>::filename() const {
  return file_helper_.filename();
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
