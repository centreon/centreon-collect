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

#include "com/centreon/connector/reporter.hh"

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/result.hh"

using namespace com::centreon::connector;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

reporter::pointer reporter::create(const shared_io_context& io_context) {
  return pointer(new reporter(io_context));
}

/**
 *  Default constructor.
 */
reporter::reporter(const shared_io_context& io_context)
    : _buffer(std::make_shared<std::string>()),
      _can_report(true),
      _reported(0),
      _io_context(io_context),
      _sout(*io_context, ::dup(STDOUT_FILENO)),
      _writing(false) {}

/**
 *  Destructor.
 */
reporter::~reporter() noexcept {
  log::core()->info("connector reported {} check results to monitoring engine",
                    _reported);
}

/**
 *  Error event on the handle.
 *
 *  @param[in] h Unused.
 */
void reporter::error() {
  _can_report = false;
}

/**
 *  Report check result.
 *
 *  @param[in] r Check result.
 */
void reporter::send_result(result const& r) {
  // Update statistics.
  ++_reported;
  log::core()->debug(
      "reporting check result #{} check:{} executed:{} exit code:{} output:{} "
      "error:{}",
      _reported, r.get_command_id(), r.get_executed(), r.get_exit_code(),
      r.get_output(), r.get_error());

  // Build packet.
  std::ostringstream oss;
  // Packet ID.
  oss << "3";
  oss.put('\0');
  // Command ID.
  oss << r.get_command_id();
  oss.put('\0');
  // Executed.
  oss << (r.get_executed() ? "1" : "0");
  oss.put('\0');
  // Exit code.
  oss << r.get_exit_code();
  oss.put('\0');
  // Error output.
  if (r.get_error().empty())
    oss.put(' ');
  else
    oss << r.get_error();
  oss.put('\0');
  // Standard output.
  if (r.get_output().empty())
    oss.put(' ');
  else
    oss << r.get_output();
  // Packet boundary.
  for (unsigned int i = 0; i < 4; ++i)
    oss.put('\0');
  // Append packet to write buffer.
  _buffer->append(oss.str());
  write();
}

/**
 *  Send protocol version to monitoring engine.
 *
 *  @param[in] major Major protocol version.
 *  @param[in] minor Minor protocol version.
 */
void reporter::send_version(unsigned int major, unsigned int minor) {
  // Build packet.
  log::core()->debug("sending protocol version {0}.{1} to monitoring engine",
                     major, minor);
  std::ostringstream oss;
  oss << "1";
  oss.put('\0');
  // Major.
  oss << major;
  oss.put('\0');
  // Minor.
  oss << minor;
  for (unsigned int i = 0; i < 4; ++i)
    oss.put('\0');

  // Send packet back to monitoring engine.
  _buffer->append(oss.str());
  write();
}

/**
 *  Send data to the monitoring engine.
 *
 *  @param[in] h Handle.
 */
void reporter::write() {
  if (_writing) {
    return;
  }
  if (!_buffer->empty()) {
    _writing = true;
    std::shared_ptr<std::string> buff = _buffer;
    _buffer = std::make_shared<std::string>();
    asio::async_write(_sout, asio::buffer(*buff),
                      [buff, me = shared_from_this()](
                          const boost::system::error_code error, std::size_t) {
                        if (error) {
                          log::core()->error("failed to write to stdout {}",
                                             error.message());
                          me->error();
                        } else {
                          me->_writing = false;
                          me->write();
                        }
                      });
  }
}
