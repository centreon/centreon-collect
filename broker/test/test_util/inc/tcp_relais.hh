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

#ifndef CCB_TEST_UTIL_TCP_RELAIS_HH
#define CCB_TEST_UTIL_TCP_RELAIS_HH

namespace com {
namespace centreon {
namespace broker {
namespace test_util {
namespace detail {
class tcp_relais_impl;
}
/**
 * @brief this class relays two connexion
 * it listens on a port
 * when it accepts a connexion it connects to a destination and do relaying
 * between both
 *
 */
class tcp_relais {
  using impl_pointer = std::shared_ptr<detail::tcp_relais_impl>;
  impl_pointer _impl;

 public:
  tcp_relais(const std::string& listen_interface,
             unsigned listen_port,
             const std::string& dest_host,
             unsigned dest_port);
  ~tcp_relais();

  void shutdown_relays();
};

}  // namespace test_util
}  // namespace broker
}  // namespace centreon
}  // namespace com

#endif  // CCB_TEST_UTIL_TCP_RELAIS_HH
