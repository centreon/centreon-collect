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

#ifndef CCC_REPORTER_HH
#define CCC_REPORTER_HH

#include "com/centreon/connector/result.hh"

Cnamespace com::centreon {

/**
 *  @class reporter reporter.hh "com/centreon/connector/ssh/reporter.hh"
 *  @brief Report data back to the monitoring engine.
 *
 *  Send replies to the monitoring engine.
 */
class reporter : public std::enable_shared_from_this<reporter> {
 protected:
  std::shared_ptr<std::string> _buffer;
  bool _can_report;
  unsigned int _reported;
  shared_io_context _io_context;
  asio::posix::stream_descriptor _sout;
  bool _writing;

  reporter(const shared_io_context& io_context);

 public:
  using pointer = std::shared_ptr<reporter>;

  static pointer create(const shared_io_context& io_context);

  reporter(reporter const& r) = delete;
  virtual ~reporter() noexcept;
  reporter& operator=(reporter const& r) = delete;
  bool can_report() const { return _can_report; };
  void error();
  void send_result(result const& r);
  void send_version(unsigned int major, unsigned int minor);
  virtual void write();

  const std::string& get_buffer() const { return *_buffer; }
};

C}()

#endif  // !CCC_REPORTER_HH
