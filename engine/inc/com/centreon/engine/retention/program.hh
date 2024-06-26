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

#include "com/centreon/common/opt.hh"
#include "com/centreon/engine/retention/object.hh"

using com::centreon::common::opt;

namespace com::centreon::engine {

namespace retention {
class program : public object {
 public:
  program();
  program(program const& right);
  ~program() throw() override;
  program& operator=(program const& right);
  bool operator==(program const& right) const throw();
  bool operator!=(program const& right) const throw();
  bool set(char const* key, char const* value) override;

  opt<bool> const& active_host_checks_enabled() const throw();
  opt<bool> const& active_service_checks_enabled() const throw();
  opt<bool> const& check_host_freshness() const throw();
  opt<bool> const& check_service_freshness() const throw();
  opt<bool> const& enable_event_handlers() const throw();
  opt<bool> const& enable_flap_detection() const throw();
  opt<bool> const& enable_notifications() const throw();
  opt<std::string> const& global_host_event_handler() const throw();
  opt<std::string> const& global_service_event_handler() const throw();
  opt<unsigned long> const& modified_host_attributes() const throw();
  opt<unsigned long> const& modified_service_attributes() const throw();
  opt<unsigned long> const& next_comment_id() const throw();
  opt<unsigned long> const& next_downtime_id() const throw();
  opt<unsigned long> const& next_event_id() const throw();
  opt<unsigned long> const& next_notification_id() const throw();
  opt<unsigned long> const& next_problem_id() const throw();
  opt<bool> const& obsess_over_hosts() const throw();
  opt<bool> const& obsess_over_services() const throw();
  opt<bool> const& passive_host_checks_enabled() const throw();
  opt<bool> const& passive_service_checks_enabled() const throw();
  opt<bool> const& process_performance_data() const throw();

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
  bool _set_next_problem_id(unsigned long value);
  bool _set_obsess_over_hosts(bool value);
  bool _set_obsess_over_services(bool value);
  bool _set_passive_host_checks_enabled(bool value);
  bool _set_passive_service_checks_enabled(bool value);
  bool _set_process_performance_data(bool value);

  opt<bool> _active_host_checks_enabled;
  opt<bool> _active_service_checks_enabled;
  opt<bool> _check_host_freshness;
  opt<bool> _check_service_freshness;
  opt<bool> _enable_event_handlers;
  opt<bool> _enable_flap_detection;
  opt<bool> _enable_notifications;
  opt<std::string> _global_host_event_handler;
  opt<std::string> _global_service_event_handler;
  opt<unsigned long> _modified_host_attributes;
  opt<unsigned long> _modified_service_attributes;
  opt<unsigned long> _next_comment_id;
  opt<unsigned long> _next_downtime_id;
  opt<unsigned long> _next_event_id;
  opt<unsigned long> _next_notification_id;
  opt<unsigned long> _next_problem_id;
  opt<bool> _obsess_over_hosts;
  opt<bool> _obsess_over_services;
  opt<bool> _passive_host_checks_enabled;
  opt<bool> _passive_service_checks_enabled;
  opt<bool> _process_performance_data;
  static setters const _setters[];
};

typedef std::shared_ptr<program> program_ptr;
}  // namespace retention

}  // namespace com::centreon::engine

#endif  // !CCE_RETENTION_PROGRAM_HH
