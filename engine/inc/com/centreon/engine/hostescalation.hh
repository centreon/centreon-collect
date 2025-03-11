/**
 * Copyright 2011-2024 Centreon
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
#ifndef CCE_HOSTESCALATION_HH
#define CCE_HOSTESCALATION_HH
#include "com/centreon/engine/escalation.hh"

/* Forward declaration. */
namespace com::centreon::engine {
class host;
class hostescalation;
}  // namespace com::centreon::engine

typedef std::unordered_multimap<
    std::string,
    std::shared_ptr<com::centreon::engine::hostescalation>>
    hostescalation_mmap;

namespace com::centreon::engine {
namespace configuration {
class Hostescalation;
}

class hostescalation : public escalation {
 public:
  hostescalation(std::string const& host_name,
                 uint32_t first_notification,
                 uint32_t last_notification,
                 double notification_interval,
                 std::string const& escalation_period,
                 uint32_t escalate_on,
                 const size_t key);
  ~hostescalation() override = default;

  std::string const& get_hostname() const;
  bool is_viable(int state, uint32_t notification_number) const override;
  void resolve(uint32_t& w, uint32_t& e) override;

  bool matches(const configuration::Hostescalation& obj) const;

  static hostescalation_mmap hostescalations;

 private:
  std::string _hostname;
};

}  // namespace com::centreon::engine

#endif  // !CCE_HOSTESCALATION_HH
