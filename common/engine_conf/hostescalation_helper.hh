/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#ifndef CCE_CONFIGURATION_HOSTESCALATION
#define CCE_CONFIGURATION_HOSTESCALATION

#include "common/engine_conf/message_helper.hh"

// A forward declaration is enough here since spdlog::logger is only used behind
// a std::shared_ptr.
namespace spdlog {
class logger;
}

namespace com::centreon::engine::configuration {

size_t hostescalation_key(const Hostescalation& he);

/**
 * @brief Convert an ActionHostEscalationOn option bitmask
 * (`escalation_options()`) into the notification_flag bitmask used by the
 * notification library (down→down, unreachable→unreachable, recovery→up).
 * Shared by Engine (applier, hostescalation::matches) and Broker (broker_cache).
 *
 * @param escalation_options A raw ActionHostEscalationOn bitmask.
 * @return The equivalent notification_flag bitmask.
 */
uint32_t host_escalation_options_to_flags(uint32_t escalation_options);

/**
 * @brief Helper for the Hostescalation message. The helper is instanciated
 * just after a message is created. It provides default values for it and also
 * several methods to help the developer to fill the message fields.
 */
class hostescalation_helper : public message_helper {
  void _init();

 public:
  hostescalation_helper(Hostescalation* obj);
  ~hostescalation_helper() noexcept = default;
  void check_validity(error_cnt& err) const override;

  bool hook(std::string_view key, std::string_view value) override;
  static void expand(
      configuration::State& s,
      configuration::error_cnt& err,
      const absl::flat_hash_map<std::string_view, configuration::Hostgroup*>&
          m_hostgroups);
  static void resolve(
      const Hostescalation& he,
      const absl::flat_hash_map<std::string_view, bool>& hosts,
      const absl::flat_hash_set<std::string_view>& contactgroups,
      const absl::flat_hash_set<std::string_view>& timeperiods,
      error_cnt& err,
      const std::shared_ptr<spdlog::logger>& logger);
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_HOSTESCALATION */
