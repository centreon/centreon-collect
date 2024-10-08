/*
** Copyright 2011-2013 Centreon
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

#ifndef CC_LOGGING_BACKEND_HH
#define CC_LOGGING_BACKEND_HH

#include <mutex>
#include <string>

namespace com::centreon::logging {

enum time_precision { none = 0, microsecond = 1, millisecond = 2, second = 3 };

/**
 *  @class backend backend.hh "com/centreon/logging/backend.hh"
 *  @brief Base logging backend class.
 *
 *  This class defines an interface to create logger backend, to
 *  log data into many different objects.
 */
class backend {
 public:
  backend(bool is_sync = true,
          bool show_pid = true,
          time_precision show_timestamp = second,
          bool show_thread_id = false);
  backend(backend const& right);
  virtual ~backend() noexcept;
  backend& operator=(backend const& right);
  virtual void close() noexcept = 0;
  virtual bool enable_sync() const;
  virtual void enable_sync(bool enable);
  virtual void log(uint64_t types, uint32_t verbose, char const* msg) noexcept;
  virtual void log(uint64_t types,
                   uint32_t verbose,
                   char const* msg,
                   uint32_t size) noexcept = 0;
  virtual void open() = 0;
  virtual void reopen() = 0;
  virtual bool show_pid() const;
  virtual void show_pid(bool enable);
  virtual time_precision show_timestamp() const;
  virtual void show_timestamp(time_precision val);
  virtual bool show_thread_id() const;
  virtual void show_thread_id(bool enable);

 protected:
  std::string _build_header();

  bool _is_sync;
  mutable std::recursive_mutex _lock;
  bool _show_pid;
  time_precision _show_timestamp;
  bool _show_thread_id;

 protected:
  void _internal_copy(backend const& right);
};

}  // namespace com::centreon::logging

#endif  // !CC_LOGGING_BACKEND_HH
