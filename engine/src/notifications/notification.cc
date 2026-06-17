/**
 * Copyright 2019-2026 Centreon (https://www.centreon.com/)
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
#include "engine/src/notifications/notification.hh"

#include <algorithm>

namespace com::centreon::engine::notifications {

notification::notification(reason_type type,
                           std::string const& author,
                           std::string const& message,
                           uint32_t options,
                           uint64_t notification_id,
                           uint32_t notification_number,
                           uint32_t notification_interval,
                           bool escalated,
                           const std::set<std::string>& notified_contacts)
    : _type{type},
      _author{author},
      _message{message},
      _options{options},
      _id{notification_id},
      _number{notification_number},
      _escalated{escalated},
      _interval{notification_interval},
      _notified_contact{notified_contacts} {}

reason_type notification::get_reason() const {
  return _type;
}

uint32_t notification::get_notification_interval() const {
  return _interval;
}

/**
 * @brief Return a boolean telling if this notification has been sent to the
 * given user.
 *
 * @param user The name of the user.
 *
 * @return a boolean.
 */
bool notification::sent_to(const std::string& user) const {
  return std::find(_notified_contact.begin(), _notified_contact.end(), user) !=
         _notified_contact.end();
}

/**
 * @brief insert contacts notified.
 *
 * @param contact_notified The names of users notified.
 */
void notification::add_contacts(const std::set<std::string>& contact_notified) {
  _notified_contact.insert(contact_notified.begin(), contact_notified.end());
}

/**
 * @brief Return a list of contact notified
 *
 * @return contacts_notified.
 */
const std::set<std::string>& notification::get_contacts() const {
  return _notified_contact;
}

/**
 *  operator<< to dump a notification in a stream
 *
 * @param os The output stream
 * @param obj The notification to dump.
 *
 * @return The output stream
 */
std::ostream& operator<<(std::ostream& os, notification const& obj) {
  os << "type: " << obj._type << ", author: " << obj._author
     << ", options: " << obj._options << ", escalated: " << obj._escalated
     << ", id: " << obj._id << ", number: " << obj._number
     << ", interval: " << obj._interval << ", contacts: ";

  for (auto& c : obj._notified_contact)
    os << c << ",";
  os << "\n";
  return os;
}

}  // namespace com::centreon::engine::notifications
