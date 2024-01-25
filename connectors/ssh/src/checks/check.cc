/**
 * Copyright 2011-2019 Centreon
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

#include "com/centreon/connector/log.hh"
#include "com/centreon/exceptions/basic.hh"

#include "com/centreon/connector/ssh/checks/check.hh"
#include "com/centreon/connector/ssh/sessions/session.hh"

using namespace com::centreon::connector::ssh::checks;

namespace com::centreon::connector::ssh::checks {
std::ostream& operator<<(std::ostream& os, const check& chk) {
  chk.dump(os);
  return os;
}

}  // namespace com::centreon::connector::ssh::checks

/**
 * @brief Construct a new check::check object
 *
 * @param session     Session on which a channel will be opened.
 * @param cmd_id      Command ID.
 * @param cmds        Commands to execute.
 * @param tmt         Command timeout.
 * @param skip_stdout Ignore all or first n output lines.
 * @param skip_stderr Ignore all or first n error lines.
 */
check::check(const sessions::session::pointer& session,
             unsigned long long cmd_id,
             string_list const& cmds,
             const time_point& tmt,
             int skip_stdout,
             int skip_stderr)
    : _channel(nullptr),
      _cmd_id(cmd_id),
      _cmds(cmds),
      _timeout(tmt),
      _session(session),
      _skip_stderr(skip_stderr),
      _skip_stdout(skip_stdout),
      _step(e_step::chan_open) {
  log::core()->debug("::check this:{}", *this);
}

/**
 *  Destructor.
 */
check::~check() noexcept {
  if (_channel) {
    // Close channel (or at least try to). Here libssh2 sucks. When
    // the close request is received on the remote end, the SSH server
    // closes the pipes it opened with the target process. It then
    // waits for it to exits (more or less forced by SIGPIPE if
    // process writes). However if process does not write we will hang
    // until it exits (which could be like forever).
    _session->async_wait(
        [channel = _channel]() { return libssh2_channel_close(channel); },
        [channel = _channel](int) { libssh2_channel_free(channel); },
        system_clock::now() + std::chrono::minutes(1), "check::~check");
  }
  log::core()->debug("~check this:{}", *this);
}

void check::dump(std::ostream& os) const {
  time_t timeout = system_clock::to_time_t(_timeout);
  os << "c this:" << this << " cmd id:" << _cmd_id
     << " timeout: " << ctime(&timeout) << " cmds: {";
  for (const std::string& cmd : _cmds) {
    os << "\" " << cmd << " \",";
  }
  os << '}';
}

/**
 *  Start executing a check.
 *
 */
void check::_execute() {
  // Log message.
  log::core()->debug("check {0} has ID {1}", static_cast<void*>(this), _cmd_id);

  _process();
}

/**
 *  Can perform action on channel.
 *
 */
void check::_process() {
  switch (_step) {
    case e_step::chan_open:
      log::core()->info("attempting to open channel for check {}", _cmd_id);
      _open();
      // if (!_open()) {
      //   log::core()->info("check {} channel was successfully opened",
      //   _cmd_id); _step = e_step::chan_exec; _process();
      // } else {
      //   log::core()->error("fail to open channel for creds:{} for check {}",
      //                      _session->get_credentials(), _cmd_id);
      //   _callback({_cmd_id, -1, "fail to open channel"});
      // }
      break;
    case e_step::chan_exec:
      log::core()->info("attempting to execute check {}", _cmd_id);
      _exec();
      break;
    case e_step::chan_read_stdout:
      log::core()->info("reading check {} result from channel", _cmd_id);
      _read(true);
      break;
    case e_step::chan_read_stderr:
      log::core()->info("reading check {} result from channel", _cmd_id);
      _read(false);
      break;
    case e_step::chan_close: {
      log::core()->info("attempting to close check {} channel", _cmd_id);
      _close();
    } break;
    default:
      throw basic_error() << "channel requested to run at invalid step";
  }
}

/**
 *  Attempt to open a channel.
 *
 *  @return true while the channel was not successfully opened.
 */
void check::_open() {
  _session->async_wait(
      [me = shared_from_this()]() {
        return me->_session->new_channel(me->_channel);
      },
      [me = shared_from_this(), this](int retval) {
        if (retval == 0) {
          log::core()->info("check {} channel was successfully opened",
                            _cmd_id);
          _step = e_step::chan_exec;
          _process();
        } else {
          char* msg;
          libssh2_session_last_error(_session->get_libssh2_session(), &msg,
                                     nullptr, 0);
          log::core()->error(
              "fail to open channel for creds:{} for check {} {}",
              _session->get_credentials(), _cmd_id, msg);
          _callback({_cmd_id, -1, "fail to open channel"});
        }
      },
      _timeout, "check::_open");
}

/**
 *  Attempt to close channel.
 *
 *  @return true while channel was not closed properly.
 */
void check::_close() {
  // Attempt to close channel.
  _session->async_wait(
      [me = shared_from_this()]() {
        return libssh2_channel_close(me->_channel);
      },
      [me = shared_from_this()](int retval) { me->_close_handler(retval); },
      _timeout, "check::_close");
}

void check::_close_handler(int retval) {
  if (retval) {
    char* msg;
    libssh2_session_last_error(_session->get_libssh2_session(), &msg, nullptr,
                               0);
    log::core()->error(
        "fail to close channel {} for creds:{} for check {} : {}",
        _cmds.front(), _session->get_credentials(), _cmd_id, msg);
    _callback(
        {_cmd_id, -1, "fail to close channel " + _cmds.front() + " " + msg});
  }
  // Close succeeded.
  else {
    // Get exit status.
    int exitcode(libssh2_channel_get_exit_status(_channel));

    // Free channel.
    libssh2_channel_free(_channel);
    _channel = nullptr;

    // Method should not be called again.
    retval = false;

    if (_skip_stdout != -1)
      _skip_data(_stdout, _skip_stdout);
    if (_skip_stderr != -1)
      _skip_data(_stderr, _skip_stderr);

    // Send results to parent process.
    _cmds.pop_front();
    _callback({_cmd_id, exitcode, _stdout, _stderr});
    if (!_cmds.empty()) {
      _step = e_step::chan_open;
      _process();
    }
  }
}

/**
 *  Attempt to execute the command.
 *
 */
void check::_exec() {
  // Attempt to execute command.
  _session->async_wait(
      [me = shared_from_this()] {
        return libssh2_channel_exec(me->_channel, me->_cmds.front().c_str());
      },
      [me = shared_from_this(), this](int retval) {
        if (retval == 0) {
          log::core()->info("check {} was successfully executed", _cmd_id);
          _step = e_step::chan_read_stdout;
          _process();
        } else {
          char* msg;
          libssh2_session_last_error(_session->get_libssh2_session(), &msg,
                                     nullptr, 0);
          log::core()->error(
              "fail to execute {} for creds:{} for check {} : {}",
              _cmds.front(), _session->get_credentials(), _cmd_id, msg);
          _callback(
              {_cmd_id, -1, "fail to execute " + _cmds.front() + " " + msg});
        }
      },
      _timeout, "check::_exec");
}

/**
 *  Attempt to read command output.
 *
 */
void check::_read(bool read_stdout) {
  int stream_id = read_stdout ? 0 : SSH_EXTENDED_DATA_STDERR;

  // replace it by std::shared_ptr<char[]> when we will use C++17
  boost::shared_array<char> buff(new char[BUFSIZ + 1]);
  buff.get()[BUFSIZ] = 0;
  _session->async_wait(
      [me = shared_from_this(), stream_id, buff]() {
        return libssh2_channel_read_ex(me->_channel, stream_id, buff.get(),
                                       BUFSIZ);
      },
      [me = shared_from_this(), buff, this](int retval) {
        if (retval >= 0) {
          if (_step == e_step::chan_read_stdout) {  // stdout => stderr
            _stdout.append(buff.get(), retval);
            _step = e_step::chan_read_stderr;
          } else {  // stderr => stdout
            _stderr.append(buff.get(), retval);
            _step = e_step::chan_read_stdout;
          }
          if (libssh2_channel_eof(_channel)) {
            _step = e_step::chan_close;
          }
          _process();
        } else {
          std::string detail;
          if (retval == LIBSSH2_ERROR_TIMEOUT) {
            detail = "time out expired";
          } else {
            char* msg;
            libssh2_session_last_error(_session->get_libssh2_session(), &msg,
                                       nullptr, 0);
            detail = msg;
          }
          log::core()->error("fail to read {} for creds:{} for check {} : {}",
                             _cmds.front(), _session->get_credentials(),
                             _cmd_id, detail);
          _callback(
              {_cmd_id, -1, "fail to read " + _cmds.front() + " " + detail});
        }
      },
      _timeout, "check::_read");
}

/**
 *  Skip n lines.
 *
 *  @param[in] data    The string to truncate.
 *  @param[in] nb_line The number of lines to keep.
 *
 *  @return The first argument.
 */
std::string& check::_skip_data(std::string& data, int nb_line) {
  if (nb_line < 0)
    return data;
  if (!nb_line)
    data.clear();
  else {
    size_t pos(0);
    for (int i(0); i < nb_line && pos != std::string::npos; ++i)
      pos = data.find("\n", pos + 1);
    if (pos != std::string::npos)
      data.resize(pos);
  }
  return data;
}
