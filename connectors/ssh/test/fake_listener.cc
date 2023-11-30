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
#include <cmath>

#include "com/centreon/connector/ssh/orders/options.hh"
#include "fake_listener.hh"

using namespace com::centreon::connector::orders;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Get the callbacks information.
 *
 *  @return Callbacks information.
 */
std::list<fake_listener::callback_info> const& fake_listener::get_callbacks()
    const noexcept {
  return _callbacks;
}

/**
 *  EOF callback.
 */
void fake_listener::on_eof() {
  callback_info ci;
  ci.callback = cb_eof;
  _callbacks.push_back(ci);
}

/**
 *  Error callback.
 *
 *  @param[in] cmd_id Command ID.
 *  @param[in] msg    Error message.
 */
void fake_listener::on_error(uint64_t, const std::string&) {
  callback_info ci;
  ci.callback = cb_error;
  _callbacks.push_back(ci);
}

/**
 *  Execute callback.
 *
 *  @param[in] cmd_id      Command ID.
 *  @param[in] timeout     Timeout.
 *  @param[in] host        Host.
 *  @param[in] port        Connection port.
 *  @param[in] user        User.
 *  @param[in] password    Password.
 *  @param[in] identity    Identity file.
 *  @param[in] cmds        Commands.
 *  @param[in] skip_stdout Should stdout be skipped.
 *  @param[in] skip_stderr Should stderr be skipped.
 *  @param[in] is_ipv6     Work with IPv6.
 */
void fake_listener::on_execute(uint64_t cmd_id,
                               const time_point& timeout,
                               const std::shared_ptr<options>& opt) {
  callback_info ci;
  ci.callback = cb_execute;
  ci.cmd_id = cmd_id;
  ci.timeout = timeout;
  ci.host = opt->get_host();
  ci.port = opt->get_port();
  ci.user = opt->get_user();
  ci.password = opt->get_authentication();
  ci.identity = opt->get_identity_file();
  ci.cmds = opt->get_commands();
  ci.skip_stdout = opt->skip_stdout();
  ci.skip_stderr = opt->skip_stderr();
  ci.is_ipv6 = opt->get_ip_protocol() == options::ip_v6;
  _callbacks.push_back(ci);
}

/**
 *  Quit callback.
 */
void fake_listener::on_quit() {
  callback_info ci;
  ci.callback = cb_quit;
  _callbacks.push_back(ci);
}

/**
 *  Version callback.
 */
void fake_listener::on_version() {
  callback_info ci;
  ci.callback = cb_version;
  _callbacks.push_back(ci);
}

/**************************************
 *                                     *
 *           Global Objects.           *
 *                                     *
 **************************************/

/**
 *  Check callback_info list equality.
 *
 *  @param[in] left  First list.
 *  @param[in] right Second list.
 *
 *  @return true if both lists are equal.
 */
bool operator==(std::list<fake_listener::callback_info> const& left,
                std::list<fake_listener::callback_info> const& right) {
  bool retval(true);
  if (left.size() != right.size())
    retval = false;
  else {
    for (auto it1 = left.begin(), end1 = left.end(), it2 = right.begin();
         it1 != end1; ++it1, ++it2)
      if ((it1->callback != it2->callback) ||
          ((it1->callback == fake_listener::cb_execute) &&
           ((it1->cmd_id != it2->cmd_id) ||
            (std::abs(std::chrono::duration_cast<std::chrono::duration<int>>(
                          it1->timeout - it2->timeout)
                          .count()) >= 1.0) ||
            (it1->host != it2->host) || (it1->port != it2->port) ||
            (it1->user != it2->user) || (it1->password != it2->password) ||
            (it1->identity != it2->identity) ||
            (it1->skip_stdout != it2->skip_stdout) ||
            (it1->skip_stderr != it2->skip_stderr) ||
            (it1->is_ipv6 != it2->is_ipv6) || (it1->cmds != it2->cmds))))
        retval = false;
  }
  return retval;
}

/**
 *  Check callback_info list inequality.
 *
 *  @param[in] left  First list.
 *  @param[in] right Second list.
 *
 *  @return true if both lists are inequal.
 */
bool operator!=(std::list<fake_listener::callback_info> const& left,
                std::list<fake_listener::callback_info> const& right) {
  return !operator==(left, right);
}
