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

#ifndef CCCS_REPORTER_HH
#define CCCS_REPORTER_HH

#include <string>
#include "com/centreon/connector/ssh/checks/listener.hh"
#include "com/centreon/connector/ssh/namespace.hh"
#include "com/centreon/handle_listener.hh"

CCCS_BEGIN()

/**
 *  @class reporter reporter.hh "com/centreon/connector/ssh/reporter.hh"
 *  @brief Report data back to the monitoring engine.
 *
 *  Send replies to the monitoring engine.
 */
class reporter : public com::centreon::handle_listener {
  std::string _buffer;
  bool _can_report;
  unsigned int _reported;

 public:
  reporter();
  reporter(reporter const& r) = delete;
  ~reporter() noexcept;
  reporter& operator=(reporter const& r) = delete;
  bool can_report() const noexcept;
  void error(handle& h);
  std::string const& get_buffer() const noexcept;
  void send_result(checks::result const& r);
  void send_version(unsigned int major, unsigned int minor);
  bool want_write(handle& h);
  void write(handle& h);
};

CCCS_END()

#endif  // !CCCS_REPORTER_HH
