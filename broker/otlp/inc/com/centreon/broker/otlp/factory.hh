/**
 * Copyright 2026 Centreon
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

#ifndef CCB_OTLP_FACTORY_HH
#define CCB_OTLP_FACTORY_HH

#include "com/centreon/broker/io/factory.hh"
#include "com/centreon/broker/otlp/otlp_config.hh"

namespace com::centreon::broker::otlp {

/**
 * @brief Builds OTLP output endpoints from broker configuration.
 */
class factory : public io::factory {
 public:
  factory() = default;
  ~factory() noexcept override = default;
  factory(const factory&) = delete;
  factory& operator=(const factory&) = delete;

  bool has_endpoint(const config::endpoint& cfg,
                    io::extension* ext) const override;
  io::endpoint* new_endpoint(
      config::endpoint& cfg,
      const std::map<std::string, std::string>& global_params,
      bool& is_acceptor,
      std::shared_ptr<persistent_cache> cache) const override;

  /**
   * @brief Parse an endpoint configuration block. Exposed for testing.
   */
  static otlp_config::pointer parse_config(const config::endpoint& cfg);
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_FACTORY_HH
