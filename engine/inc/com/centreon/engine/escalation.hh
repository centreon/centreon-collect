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
#ifndef CCE_ESCALATION_HH
#define CCE_ESCALATION_HH

#include "com/centreon/engine/notifier.hh"

/* Forward declaration. */
namespace com::centreon::engine {
class timeperiod;

class escalation {
  uint32_t _first_notification;
  uint32_t _last_notification;
  double _notification_interval;
  std::string _escalation_period;
  uint32_t _escalate_on;
  contactgroup_map _contact_groups;
  const size_t _internal_key;

 public:
  escalation(uint32_t first_notification,
             uint32_t last_notification,
             double notification_interval,
             std::string const& escalation_period,
             uint32_t escalate_on,
             const size_t key);
  virtual ~escalation() noexcept = default;

  std::string const& get_escalation_period() const;
  uint32_t get_first_notification() const;
  uint32_t get_last_notification() const;
  double get_notification_interval() const;
  void set_notification_interval(double notification_interval);
  void add_escalate_on(notifier::notification_flag type);
  void remove_escalate_on(notifier::notification_flag type);
  uint32_t get_escalate_on() const;
  bool get_escalate_on(notifier::notification_flag type) const;
  void set_escalate_on(uint32_t escalate_on);
  virtual bool is_viable(int state, uint32_t notification_number) const;
  size_t internal_key() const;

  const contactgroup_map& get_contactgroups() const;
  contactgroup_map& get_contactgroups();
  virtual void resolve(uint32_t& w, uint32_t& e);

  notifier* notifier_ptr;
  timeperiod* escalation_period_ptr;
};
}  // namespace com::centreon::engine

#endif  // !CCE_ESCALATION_HH
