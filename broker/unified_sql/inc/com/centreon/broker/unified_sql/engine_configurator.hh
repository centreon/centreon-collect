/**
 * Copyright 2024 Centreon
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

#ifndef CCB_UNIFIED_SQL_ENGINE_CONFIGURATOR_HH
#define CCB_UNIFIED_SQL_ENGINE_CONFIGURATOR_HH

namespace com::centreon::broker::unified_sql {
class engine_configurator {
  const uint32_t _instance_id;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  engine_configurator(uint32_t instance_id,
                      const std::shared_ptr<spdlog::logger>& logger);
  engine_configurator(const engine_configurator&) = delete;
  engine_configurator& operator=(const engine_configurator&) = delete;
  ~engine_configurator() noexcept = default;

  void apply();
};
}  // namespace com::centreon::broker::unified_sql

#endif /* !CCB_UNIFIED_SQL_ENGINE_CONFIGURATOR_HH */
