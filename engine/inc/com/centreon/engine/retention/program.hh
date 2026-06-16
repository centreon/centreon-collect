/**
 * Copyright 2011-2013,2015 Merethis
 * Copyright 2016-2024 Centreon
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
#ifndef CCE_RETENTION_PROGRAM_HH
#define CCE_RETENTION_PROGRAM_HH

#include <optional>
#include "com/centreon/engine/retention/object.hh"

namespace com::centreon::engine {

namespace retention {
class program : public object {
 public:
  program();
  program(program const& right);
  ~program() noexcept override;
  program& operator=(program const& right);
  bool operator==(program const& right) const noexcept;
  bool operator!=(program const& right) const noexcept;
  bool set(char const* key, char const* value) override;

  std::optional<bool> const& active_host_checks_enabled() const noexcept;
  std::optional<bool> const& active_service_checks_enabled() const noexcept;
  std::optional<bool> const& check_host_freshness() const noexcept;
  std::optional<bool> const& check_service_freshness() const noexcept;
  std::optional<bool> const& enable_event_handlers() const noexcept;
  std::optional<bool> const& enable_flap_detection() const noexcept;
  std::optional<bool> const& enable_notifications() const noexcept;
  std::optional<std::string> const& global_host_event_handler() const noexcept;
  std::optional<std::string> const& global_service_event_handler()
      const noexcept;
  std::optional<unsigned long> const& modified_host_attributes() const noexcept;
  std::optional<unsigned long> const& modified_service_attributes()
      const noexcept;
  std::optional<unsigned long> const& next_comment_id() const noexcept;
  std::optional<unsigned long> const& next_downtime_id() const noexcept;
  std::optional<unsigned long> const& next_event_id() const noexcept;
  std::optional<unsigned long> const& next_notification_id() const noexcept;
  std::optional<uint64_t> const& next_problem_id() const noexcept;
  std::optional<bool> const& obsess_over_hosts() const noexcept;
  std::optional<bool> const& obsess_over_services() const noexcept;
  std::optional<bool> const& passive_host_checks_enabled() const noexcept;
  std::optional<bool> const& passive_service_checks_enabled() const noexcept;
  std::optional<bool> const& process_performance_data() const noexcept;

 private:
  struct setters {
    char const* name;
    bool (*func)(program&, char const*);
  };

  bool _set_active_host_checks_enabled(bool value);
  bool _set_active_service_checks_enabled(bool value);
  bool _set_check_host_freshness(bool value);
  bool _set_check_service_freshness(bool value);
  bool _set_enable_event_handlers(bool value);
  bool _set_enable_flap_detection(bool value);
  bool _set_enable_notifications(bool value);
  bool _set_global_host_event_handler(std::string const& value);
  bool _set_global_service_event_handler(std::string const& value);
  bool _set_modified_host_attributes(unsigned long value);
  bool _set_modified_service_attributes(unsigned long value);
  bool _set_next_comment_id(unsigned long value);
  bool _set_next_downtime_id(unsigned long value);
  bool _set_next_event_id(unsigned long value);
  bool _set_next_notification_id(unsigned long value);
  bool _set_next_problem_id(uint64_t value);
  bool _set_obsess_over_hosts(bool value);
  bool _set_obsess_over_services(bool value);
  bool _set_passive_host_checks_enabled(bool value);
  bool _set_passive_service_checks_enabled(bool value);
  bool _set_process_performance_data(bool value);

  std::optional<bool> _active_host_checks_enabled;
  std::optional<bool> _active_service_checks_enabled;
  std::optional<bool> _check_host_freshness;
  std::optional<bool> _check_service_freshness;
  std::optional<bool> _enable_event_handlers;
  std::optional<bool> _enable_flap_detection;
  std::optional<bool> _enable_notifications;
  std::optional<std::string> _global_host_event_handler;
  std::optional<std::string> _global_service_event_handler;
  std::optional<unsigned long> _modified_host_attributes;
  std::optional<unsigned long> _modified_service_attributes;
  std::optional<unsigned long> _next_comment_id;
  std::optional<unsigned long> _next_downtime_id;
  std::optional<unsigned long> _next_event_id;
  std::optional<unsigned long> _next_notification_id;
  std::optional<uint64_t> _next_problem_id;
  std::optional<bool> _obsess_over_hosts;
  std::optional<bool> _obsess_over_services;
  std::optional<bool> _passive_host_checks_enabled;
  std::optional<bool> _passive_service_checks_enabled;
  std::optional<bool> _process_performance_data;
  static setters const _setters[];
};

typedef std::shared_ptr<program> program_ptr;
}  // namespace retention

}  // namespace com::centreon::engine

#endif  // !CCE_RETENTION_PROGRAM_HH
