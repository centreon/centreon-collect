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

#ifndef CCCP_REPORTER_HH
#define CCCP_REPORTER_HH

#include <string>

#include "com/centreon/connector/perl/checks/listener.hh"
#include "com/centreon/connector/perl/namespace.hh"
#include "com/centreon/handle_listener.hh"

CCCP_BEGIN()

/**
 *  @class reporter reporter.hh "com/centreon/connector/perl/reporter.hh"
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
  ~reporter() noexcept override;
  reporter(reporter const& r) = delete;
  reporter& operator=(reporter const& r) = delete;
  bool can_report() const noexcept;
  void error(handle& h) override;
  void send_result(checks::result const& r);
  void send_version(unsigned int major, unsigned int minor);
  bool want_write(handle& h) override;
  void write(handle& h) override;
};

CCCP_END()

#endif  // !CCCP_REPORTER_HH
