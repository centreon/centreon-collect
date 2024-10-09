/**
 * Copyright 2011-2013, 2020-2022 Centreon
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

#ifndef CCB_TCP_FACTORY_HH
#define CCB_TCP_FACTORY_HH

#include "com/centreon/broker/io/factory.hh"

namespace com::centreon::broker {

namespace tcp {
/**
 *  @class factory factory.hh "com/centreon/broker/tcp/factory.hh"
 *  @brief TCP protocol factory.
 *
 *  Build TCP protocol objects.
 */
class factory : public io::factory {
  io::endpoint* _new_endpoint_bbdo_cs(
      com::centreon::broker::config::endpoint& cfg,
      bool& is_acceptor) const;

 public:
  factory() = default;
  factory(factory const& other) = delete;
  ~factory() noexcept = default;
  factory& operator=(factory const& other) = delete;
  bool has_endpoint(com::centreon::broker::config::endpoint& cfg,
                    io::extension* ext) override;
  io::endpoint* new_endpoint(
      com::centreon::broker::config::endpoint& cfg,
      const std::map<std::string, std::string>& global_params,
      bool& is_acceptor,
      std::shared_ptr<persistent_cache> cache =
          std::shared_ptr<persistent_cache>()) const override;
};
}  // namespace tcp

}  // namespace com::centreon::broker

#endif  // !CCB_TCP_FACTORY_HH
