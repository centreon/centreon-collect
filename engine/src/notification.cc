/**
 * Copyright 2019-2024 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/notification.hh"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/macros/defines.hh"
#include "com/centreon/engine/neberrors.hh"
#include "com/centreon/engine/notifier.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

notification::notification(notifier* parent,
                           notifier::reason_type type,
                           std::string const& author,
                           std::string const& message,
                           uint32_t options,
                           uint64_t notification_id,
                           uint32_t notification_number,
                           uint32_t notification_interval,
                           bool escalated,
                           const std::set<std::string>& notified_contacts)
    : _parent{parent},
      _type{type},
      _author{author},
      _message{message},
      _options{options},
      _id{notification_id},
      _number{notification_number},
      _escalated{escalated},
      _interval{notification_interval},
      _notified_contact{notified_contacts} {}

/**
 * @brief Execute the notification, that is to say, for each contact to
 * notify, the notification is sent to him.
 *
 * @param to_notify the set of contact to notify.
 *
 * @return OK on success, ERROR otherwise.
 */
int notification::execute(
    const std::unordered_set<std::shared_ptr<contact>>& to_notify) {
  uint32_t contacts_notified{0};

  struct timeval start_time;
  gettimeofday(&start_time, nullptr);

  nagios_macros* mac(get_global_macros());

  /* Grab the macro variables */
  _parent->grab_macros_r(mac);

  contact* author{nullptr};
  contact_map::const_iterator it{contact::contacts.find(_author)};
  if (it != contact::contacts.end())
    author = it->second.get();
  else {
    for (contact_map::const_iterator cit{contact::contacts.begin()},
         cend{contact::contacts.end()};
         cit != cend; ++cit) {
      if (cit->second->get_alias() == _author) {
        author = cit->second.get();
        break;
      }
    }
  }

  /* Get author and comment macros */
  mac->x[MACRO_NOTIFICATIONAUTHOR] = _author;
  mac->x[MACRO_NOTIFICATIONCOMMENT] = _message;
  if (author) {
    mac->x[MACRO_NOTIFICATIONAUTHORNAME] = author->get_name();
    mac->x[MACRO_NOTIFICATIONAUTHORALIAS] = author->get_alias();
  } else {
    mac->x[MACRO_NOTIFICATIONAUTHORNAME] = "";
    mac->x[MACRO_NOTIFICATIONAUTHORALIAS] = "";
  }

  /* set the notification type macro */
  switch (_type) {
    case notifier::reason_acknowledgement:
      mac->x[MACRO_NOTIFICATIONTYPE] = "ACKNOWLEDGEMENT";
      break;
    case notifier::reason_flappingstart:
      mac->x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGSTART";
      break;
    case notifier::reason_flappingstop:
      mac->x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGSTOP";
      break;
    case notifier::reason_flappingdisabled:
      mac->x[MACRO_NOTIFICATIONTYPE] = "FLAPPINGDISABLED";
      break;
    case notifier::reason_downtimestart:
      mac->x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMESTART";
      break;
    case notifier::reason_downtimeend:
      mac->x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMEEND";
      break;
    case notifier::reason_downtimecancelled:
      mac->x[MACRO_NOTIFICATIONTYPE] = "DOWNTIMECANCELLED";
      break;
    case notifier::reason_custom:
      mac->x[MACRO_NOTIFICATIONTYPE] = "CUSTOM";
      break;
    case notifier::reason_recovery:
      mac->x[MACRO_NOTIFICATIONTYPE] = "RECOVERY";
      break;
    default:
      mac->x[MACRO_NOTIFICATIONTYPE] = "PROBLEM";
      break;
  }

  if (_parent->get_notifier_type() == notifier::host_notification) {
    /* set the notification number macro */
    mac->x[MACRO_HOSTNOTIFICATIONNUMBER] = std::to_string(_number);

    /* The $NOTIFICATIONNUMBER$ macro is maintained for backward compatibility
     */
    mac->x[MACRO_NOTIFICATIONNUMBER] = mac->x[MACRO_HOSTNOTIFICATIONNUMBER];
    /* set the notification is escalated macro */
    mac->x[MACRO_NOTIFICATIONISESCALATED] = std::to_string(_escalated);

    /* Set the notification id macro */
    mac->x[MACRO_HOSTNOTIFICATIONID] = std::to_string(_id);
  } else {
    /* set the notification number macro */
    mac->x[MACRO_SERVICENOTIFICATIONNUMBER] = std::to_string(_number);

    /* The $NOTIFICATIONNUMBER$ macro is maintained for backward compatibility
     */
    mac->x[MACRO_NOTIFICATIONNUMBER] = mac->x[MACRO_SERVICENOTIFICATIONNUMBER];

    /* set the notification is escalated macro */
    mac->x[MACRO_NOTIFICATIONISESCALATED] = std::to_string(_escalated);

    /* Set the notification id macro */
    mac->x[MACRO_SERVICENOTIFICATIONID] = std::to_string(_id);
  }

  for (const std::shared_ptr<contact>& ctc_ptr : to_notify) {
    /* get the contact */
    auto ctc = ctc_ptr.get();

    /* grab the macro variables for this contact */
    grab_contact_macros_r(mac, ctc);

    /* clear summary macros (they are customized for each contact) */
    clear_summary_macros_r(mac);

    /* notify this contact */
    if (_parent->notify_contact(mac, ctc, _type, _author.c_str(),
                                _message.c_str(), _options, _escalated) == OK) {
      /* keep track of how many contacts were notified */
      contacts_notified++;
      _notified_contact.insert(ctc->get_name());
      if (mac->x[MACRO_NOTIFICATIONRECIPIENTS].empty())
        mac->x[MACRO_NOTIFICATIONRECIPIENTS] = ctc->get_name();
      else {
        mac->x[MACRO_NOTIFICATIONRECIPIENTS].append(",");
        mac->x[MACRO_NOTIFICATIONRECIPIENTS].append(ctc->get_name());
      }
    }
  }

  engine_logger(dbg_notifications, basic)
      << contacts_notified << " contacts were notified.";
  notifications_logger->trace("{} contacts were notified.", contacts_notified);
  return OK;
}

notifier::reason_type notification::get_reason() const {
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

namespace com::centreon::engine {
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
}  // namespace com::centreon::engine
