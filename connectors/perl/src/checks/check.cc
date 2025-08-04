/**
 * Copyright 2022 Centreon
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

#include "com/centreon/connector/perl/checks/check.hh"

#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/embedded_perl.hh"
#include "com/centreon/connector/reporter.hh"
#include "com/centreon/connector/result.hh"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::perl::checks;

/**
 * @brief keep trace of father fd that are closed by child
 *
 */
absl::flat_hash_set<int> check::_all_child_fd;

/**
 * @brief this set is used only for father to wait for all child to terminate
 * before exit
 *
 */
absl::flat_hash_set<check*> check::_active_check;

/**
 * @brief Construct a new check::check object
 *
 * @param cmd_id
 * @param cmd path to the script to execute
 * @param tmt  absolute timeout
 * @param reporter reporter that will write to engine
 * @param io_context
 */
check::check(uint64_t cmd_id,
             const std::string& cmd,
             const time_point& tmt,
             const std::shared_ptr<com::centreon::connector::reporter> reporter,
             const shared_io_context& io_context)
    : _child((pid_t)-1),
      _cmd_id(cmd_id),
      _cmd(cmd),
      _out(*io_context),
      _err(*io_context),
      _out_fd(0),
      _err_fd(0),
      _timeout(tmt),
      _out_eof(false),
      _err_eof(false),
      _exit_code_set(false),
      _timeout_timer(*io_context),
      _reporter(reporter),
      _io_context(io_context) {
  log::core()->trace("check::check {}", *this);
}

check::~check() {
  log::core()->trace("check::~check {}", *this);
  _all_child_fd.erase(_out_fd);
  _all_child_fd.erase(_err_fd);
  _active_check.erase(this);
}

/**
 * @brief static method that close mostly father fds
 *
 */
void check::close_all_father_fd() {
  for (int fd : _all_child_fd) {
    ::close(fd);
  }
}

/**
 *  Execute a Perl script.
 *
 *  @return Process ID.
 */
pid_t check::execute() {
  try {
    // Run process.
    int fds[3];
    _child = embedded_perl::instance().run(_cmd, fds, _io_context);
    ::close(fds[0]);
    _all_child_fd.insert(fds[1]);
    _all_child_fd.insert(fds[2]);
    _out_fd = fds[1];
    _err_fd = fds[2];
    _out.assign(fds[1]);
    _err.assign(fds[2]);
    _start_read_out();
    _start_read_err();

    // Store command ID.
    log::core()->debug("execute {} _out_fd={} _err_fd={}", *this, _out_fd,
                       _err_fd);

    _active_check.insert(this);

    _timeout_timer.expires_at(_timeout);
    _timeout_timer.async_wait(
        [me = shared_from_this()](const boost::system::error_code& err) {
          me->on_timeout(err, false);
        });
  } catch (const std::exception& e) {
    log::core()->error("{} fail to start perl : {}", *this, e.what());
    throw;
  }
  return _child;
}

/**
 * @brief start read on child's stdout
 *
 */
void check::_start_read_out() {
  _out.async_read_some(
      asio::buffer(_out_buff),
      [me = shared_from_this(), this](const boost::system::error_code& err,
                                      std::size_t bytes_transferred) {
        if (err) {
          if (err.value() == asio::error::eof) {
            log::core()->debug("{} stdout eof:{}", *this, err.message());
          } else {
            log::core()->error("{} stdout read error:{}", *this, err.message());
          }
          _out_eof = true;
          _send_result();
          return;
        }
        log::core()->debug("{} stdout read {} bytes", *this, bytes_transferred);
        _stdout.append(_out_buff.data(), bytes_transferred);
        _start_read_out();
      });
}

/**
 * @brief start read on child's stderr
 *
 */
void check::_start_read_err() {
  _err.async_read_some(
      asio::buffer(_err_buff),
      [me = shared_from_this(), this](const boost::system::error_code& err,
                                      std::size_t bytes_transferred) {
        if (err) {
          if (err.value() == asio::error::eof) {
            log::core()->debug("{} stderr eof:{}", *this, err.message());
          } else {
            log::core()->error("{} stderr read error:{}", *this, err.message());
          }
          _err_eof = true;
          _send_result();
          return;
        }
        log::core()->debug("{} stderr read {} bytes", *this, bytes_transferred);
        _stderr.append(_err_buff.data(), bytes_transferred);
        _start_read_err();
      });
}

/**
 *  Called when check timeout occurs.
 *
 *  @param[in] final if true child process will receive a sigkill instead of
 * sigterm
 */
void check::on_timeout(const boost::system::error_code& err, bool final) {
  if (err) {
    return;
  }

  if (_child <= 0)
    return;

  _stderr += " time out";

  if (final) {
    log::core()->error("{} reached timeout kill -9", *this);
    // Send SIGKILL (not catchable, not ignorable).
    kill(_child, SIGKILL);
  } else {
    log::core()->error("{} reached timeout kill -15", *this);
    // Try graceful shutdown.
    kill(_child, SIGTERM);

    _timeout_timer.expires_after(std::chrono::seconds(10));
    _timeout_timer.async_wait(
        [me = shared_from_this()](const boost::system::error_code& err) {
          me->on_timeout(err, true);
        });
  }
}

/**
 * @brief set exit code of the child process
 *
 * @param exit_code
 */
void check::set_exit_code(int exit_code) {
  _exit_code_set = true;
  _exit_code = exit_code;
  log::core()->debug("{} exit_code={}", *this, exit_code);
  _send_result();
}

/**
 * @brief if exit_code is set and if we have received an eof on child stdin and
 * stderr this method send result to engine
 *
 */
void check::_send_result() {
  if (_err_eof && _out_eof && _exit_code_set) {
    _reporter->send_result({_cmd_id, _exit_code, _stdout, _stderr});
    _timeout_timer.cancel();
    _active_check.erase(this);
  }
}

/**
 * @brief dump used by << operator
 *
 * @param s
 */
void check::dump(std::ostream& s) const {
  s << "check this=" << this << " , cmd_id=" << _cmd_id << " ,pid=" << _child
    << " cmd=" << _cmd;
}
