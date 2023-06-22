/*
** Copyright 2023 Centreon
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

#ifndef CCCM_PROCESS_STAT_HH
#define CCCM_PROCESS_STAT_HH

#include <time.h>

#include <chrono>
#include <string>

#include <boost/exception/exception.hpp>
#include <boost/exception/info.hpp>

#include "namespace.hh"

CCCM_BEGIN()

class process_stat {
  pid_t _pid;
  std::string _cmdline;
  unsigned _num_threads;

  // io file
  uint64_t _rchar;
  uint64_t _wchar;
  uint64_t _read_bytes;
  uint64_t _write_bytes;

  std::chrono::system_clock::duration _utime;
  std::chrono::system_clock::duration _stime;

  std::chrono::system_clock::time_point _starttime;

  uint64_t _vm_size;
  uint64_t _res_size;
  uint64_t _shared_size;

 public:
  struct exception : virtual std::exception, virtual boost::exception {};
  using errinfo_bad_info_format =
      boost::error_info<struct errinfo_bad_info_format_, std::string>;

  process_stat(pid_t process_id);

  pid_t pid() const { return _pid; }

  /**
   * @brief command line exe with arguments
   *
   * @return * const std::string
   */
  const std::string cmdline() const { return _cmdline; }

  /**
   * @brief process thread count
   *
   * @return unsigned
   */
  unsigned num_threads() const { return _num_threads; }

  /**
   * The number of bytes which this task has caused to
   * be read from storage.  This is simply the sum of
   * bytes which this process passed to read(2) and
   * similar system calls.  It includes things such as
   * terminal I/O and is unaffected by whether or not
   * actual physical disk I/O was required (the read
   * might have been satisfied from pagecache). *
   * @return uint64_t
   */
  uint64_t rchar() const { return _rchar; }

  /**
   * The number of bytes which this task has caused, or
   * shall cause to be written to disk.  Similar caveats
   * apply here as with rchar.
   *
   * @return uint64_t
   */
  uint64_t wchar() const { return _wchar; }

  /**
   * Attempt to count the number of bytes which this
   * process really did cause to be fetched from the
   * storage layer.  This is accurate for block-backed
   * filesystems.
   *
   * @return uint64_t
   */
  uint64_t read_bytes() const { return _read_bytes; }

  /**
   * Attempt to count the number of bytes which this
   * process caused to be sent to the storage layer.
   *
   * @return uint64_t
   */
  uint64_t write_bytes() const { return _write_bytes; }

  /**
   * Amount of time that this process has been scheduled
   * in user mode. This includes guest time,
   * guest_time (time spent running a virtual CPU, see
   * below), so that applications that are not aware of
   * the guest time field do not lose that time from
   * their calculations.
   *
   * @return std::chrono::system_clock::duration
   */
  const std::chrono::system_clock::duration& utime() const { return _utime; }

  /**
   * Amount of time that this process has been scheduled
   * in kernel mode
   *
   * @return std::chrono::system_clock::duration
   */
  const std::chrono::system_clock::duration& stime() const { return _stime; }

  /**
   * @brief The time the process started after system boot.
   *
   * @return const std::chrono::system_clock::time_point&
   */
  const std::chrono::system_clock::time_point& starttime() const {
    return _starttime;
  }

  /**
   * @brief total program size
   *
   * @return uint64_t size in bytes
   */
  uint64_t vm_size() const { return _vm_size; }

  /**
   * @brief resident set size
   *
   * @return uint64_t size in bytes
   */
  uint64_t res_size() const { return _res_size; }

  /**
   * @brief shared size
   *
   * @return uint64_t size in bytes
   */
  uint64_t shared_size() const { return _shared_size; }

  template <class protobuf_class>
  void to_protobuff(protobuf_class& dest) const;
};

/**
 * @brief fill a process_stat protobuf message
 *
 * @tparam protobuf_class
 * @param dest
 */
template <class protobuf_class>
void process_stat::to_protobuff(protobuf_class& dest) const {
  dest.set_pid(_pid);
  dest.set_cmdline(_cmdline);
  dest.set_nb_thread(_num_threads);
  dest.set_start_time(std::chrono::duration_cast<std::chrono::seconds>(
                          _starttime.time_since_epoch())
                          .count());
  dest.set_io_rchar(_rchar);
  dest.set_io_wchar(_wchar);
  dest.set_io_read_bytes(_read_bytes);
  dest.set_io_write_bytes(_write_bytes);
  dest.set_user_time(
      std::chrono::duration_cast<std::chrono::seconds>(_utime).count());
  dest.set_kernel_time(
      std::chrono::duration_cast<std::chrono::seconds>(_stime).count());
  dest.set_vm_size(_vm_size);
  dest.set_res_size(_res_size);
  dest.set_shared_size(_shared_size);
}

CCCM_END()

#endif
