/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2024 Centreon
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
#ifndef CCE_RETENTION_CONTACT_HH
#define CCE_RETENTION_CONTACT_HH

#include <optional>
#include "com/centreon/engine/customvariable.hh"
#include "com/centreon/engine/retention/object.hh"

namespace com::centreon::engine {

namespace retention {
class contact : public object {
 public:
  contact();
  contact(contact const& right);
  ~contact() noexcept override;
  contact& operator=(contact const& right);
  bool operator==(contact const& right) const noexcept;
  bool operator!=(contact const& right) const noexcept;
  bool set(char const* key, char const* value) override;

  std::string const& contact_name() const noexcept;
  map_customvar const& customvariables() const noexcept;
  std::optional<std::string> const& host_notification_period() const noexcept;
  std::optional<bool> const& host_notifications_enabled() const noexcept;
  std::optional<time_t> const& last_host_notification() const noexcept;
  std::optional<time_t> const& last_service_notification() const noexcept;
  std::optional<unsigned long> const& modified_attributes() const noexcept;
  std::optional<unsigned long> const& modified_host_attributes() const noexcept;
  std::optional<unsigned long> const& modified_service_attributes()
      const noexcept;
  std::optional<std::string> const& service_notification_period()
      const noexcept;
  std::optional<bool> const& service_notifications_enabled() const noexcept;

 private:
  struct setters {
    char const* name;
    bool (*func)(contact&, char const*);
  };

  bool _set_contact_name(std::string const& value);
  bool _set_host_notification_period(std::string const& value);
  bool _set_host_notifications_enabled(bool value);
  bool _set_last_host_notification(time_t value);
  bool _set_last_service_notification(time_t value);
  bool _set_modified_attributes(unsigned long value);
  bool _set_modified_host_attributes(unsigned long value);
  bool _set_modified_service_attributes(unsigned long value);
  bool _set_service_notification_period(std::string const& value);
  bool _set_service_notifications_enabled(bool value);

  std::string _contact_name;
  map_customvar _customvariables;
  std::optional<std::string> _host_notification_period;
  std::optional<bool> _host_notifications_enabled;
  std::optional<time_t> _last_host_notification;
  std::optional<time_t> _last_service_notification;
  std::optional<unsigned long> _modified_attributes;
  std::optional<unsigned long> _modified_host_attributes;
  std::optional<unsigned long> _modified_service_attributes;
  std::optional<std::string> _service_notification_period;
  std::optional<bool> _service_notifications_enabled;
  static setters const _setters[];
};

typedef std::shared_ptr<contact> contact_ptr;
typedef std::list<contact_ptr> list_contact;
}  // namespace retention

}  // namespace com::centreon::engine

#endif  // !CCE_RETENTION_CONTACT_HH
