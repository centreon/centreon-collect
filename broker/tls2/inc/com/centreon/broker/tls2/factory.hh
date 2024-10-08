/*
** Copyright 2013 Centreon
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

#ifndef CCB_TLS_FACTORY_HH
#define CCB_TLS_FACTORY_HH

#include "com/centreon/broker/io/factory.hh"

namespace com::centreon::broker {

namespace tls2 {
/**
 *  @class factory factory.hh "com/centreon/broker/tls2/factory.hh"
 *  @brief TLS security layer factory.
 *
 *  Build TLS security objects.
 */
class factory : public io::factory {
 public:
  factory() = default;
  ~factory() = default;
  factory(factory const& other) = delete;
  factory& operator=(factory const& other) = delete;
  bool has_endpoint(config::endpoint& cfg, io::extension* ext) override;
  io::endpoint* new_endpoint(
      config::endpoint& cfg,
      const absl::flat_hash_map<std::string, std::string>& global_params,
      bool& is_acceptor,
      std::shared_ptr<persistent_cache> cache =
          std::shared_ptr<persistent_cache>()) const override;
  std::shared_ptr<io::stream> new_stream(
      std::shared_ptr<io::stream> to,
      bool is_acceptor,
      const std::unordered_map<std::string, std::string>& options) override;
};
}  // namespace tls2

}  // namespace com::centreon::broker

#endif  // !CCB_TLS_FACTORY_HH
