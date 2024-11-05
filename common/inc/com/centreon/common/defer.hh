/*
** Copyright 2022 Centreon
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

#ifndef _COM_CENTREON_DEFER_HH_
#define _COM_CENTREON_DEFER_HH_

namespace com::centreon::common {

/**
 * @brief this function executes the handler action in delay
 *
 * @tparam handler_type
 * @param io_context
 * @param delay the delay when to execute handler
 * @param handler job to do
 */
template <class handler_type>
void defer(const std::shared_ptr<asio::io_context>& io_context,
           const std::chrono::system_clock::duration& delay,
           handler_type&& handler) {
  std::shared_ptr<asio::system_timer> timer(
      std::make_shared<asio::system_timer>(*io_context));
  timer->expires_after(delay);
  timer->async_wait([io_context, timer, m_handler = std::move(handler)](
                        const boost::system::error_code& err) {
    if (!err) {
      m_handler();
    }
  });
};

/**
 * @brief this function executes the handler action in delay
 *
 * @tparam handler_type
 * @param io_context
 * @param tp the time point when to execute handler
 * @param handler job to do
 */
template <class handler_type>
void defer(const std::shared_ptr<asio::io_context>& io_context,
           const std::chrono::system_clock::time_point& tp,
           handler_type&& handler) {
  std::shared_ptr<asio::system_timer> timer(
      std::make_shared<asio::system_timer>(*io_context));
  timer->expires_at(tp);
  timer->async_wait([io_context, timer, m_handler = std::move(handler)](
                        const boost::system::error_code& err) {
    if (!err) {
      m_handler();
    }
  });
};

}  // namespace com::centreon::common

#endif
