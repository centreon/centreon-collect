/**
* Copyright 2011-2013 Centreon
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

#include "com/centreon/logging/syslogger.hh"
#include <syslog.h>
#include <cstdlib>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/misc/stringifier.hh"

using namespace com::centreon::logging;

/**
 *  Constructor.
 *
 *  @param[in] id              The id prepended to every message.
 *  @param[in] facility        This is the syslog facility.
 *  @param[in] is_sync         Enable synchronization.
 *  @param[in] show_pid        Enable show pid.
 *  @param[in] show_timestamp  Enable show timestamp.
 *  @param[in] show_thread_id  Enable show thread id.
 */
syslogger::syslogger(std::string const& id,
                     int facility,
                     bool is_sync,
                     bool show_pid,
                     time_precision show_timestamp,
                     bool show_thread_id)
    : backend(is_sync, show_pid, show_timestamp, show_thread_id),
      _facility(facility),
      _id(id) {
  open();
}

/**
 *  Default destructor.
 */
syslogger::~syslogger() noexcept {
  close();
}

/**
 *  Close syslog.
 */
void syslogger::close() noexcept {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  closelog();
}

/**
 *  Write message into the syslog.
 *  @remark This method is thread safe.
 *
 *  @param[in] type     Logging types.
 *  @param[in] verbose  Verbosity level.
 *  @param[in] msg      The message to write.
 *  @param[in] size     The message's size.
 */
void syslogger::log(uint64_t types,
                    uint32_t verbose,
                    char const* msg,
                    uint32_t size) noexcept {
  (void)types;
  (void)verbose;
  (void)size;

  misc::stringifier header;
  _build_header(header);

  std::lock_guard<std::recursive_mutex> lock(_lock);
  syslog(LOG_ERR, "%s%s", header.data(), msg);
}

/**
 *  Open syslog.
 */
void syslogger::open() {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  openlog(_id.c_str(), 0, _facility);
}

/**
 *  Close and open syslog.
 */
void syslogger::reopen() {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  closelog();
  openlog(_id.c_str(), 0, _facility);
}
