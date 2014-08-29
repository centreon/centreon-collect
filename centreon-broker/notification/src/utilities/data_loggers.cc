/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/broker/notification/utilities/data_loggers.hh"
#define LOG_MEMBER(klass, getter)\
  << #klass "::" #getter "() : " \
  << (obj.getter()) << "; "

using namespace com::centreon::broker;

logging::temp_logger& logging::operator<<(logging::temp_logger const& left,
                                          notification::node_id const& obj) throw()
{
  logging::temp_logger& tmp = const_cast<logging::temp_logger&>(left);
  tmp LOG_MEMBER(node_id, get_host_id)
      LOG_MEMBER(node_id, get_service_id);
  return (tmp);
}

logging::temp_logger& logging::operator<<(logging::temp_logger const& left,
                                          notification::command const& obj) throw() {
  logging::temp_logger& tmp = const_cast<logging::temp_logger&>(left);
  tmp LOG_MEMBER(command, get_name);
  return (tmp);
}

logging::temp_logger& logging::operator<<(logging::temp_logger const& left,
                                          notification::downtime const& obj) throw() {
  logging::temp_logger& tmp = const_cast<logging::temp_logger&>(left);
  tmp LOG_MEMBER(downtime, get_entry_time)
      LOG_MEMBER(downtime, get_host_id)
      LOG_MEMBER(downtime, get_service_id)
      LOG_MEMBER(downtime, get_author)
      LOG_MEMBER(downtime, get_cancelled)
      LOG_MEMBER(downtime, get_deletion_time)
      LOG_MEMBER(downtime, get_duration)
      LOG_MEMBER(downtime, get_end_time)
      LOG_MEMBER(downtime, get_fixed)
      LOG_MEMBER(downtime, get_start_time)
      LOG_MEMBER(downtime, get_actual_start_time)
      LOG_MEMBER(downtime, get_actual_end_time)
      LOG_MEMBER(downtime, get_started)
      LOG_MEMBER(downtime, get_triggered_by)
      LOG_MEMBER(downtime, get_type);
  return (tmp);
}

logging::temp_logger& logging::operator<<(logging::temp_logger const& left,
                                          notification::escalation const& obj) throw() {
  logging::temp_logger& tmp = const_cast<logging::temp_logger&>(left);
  tmp LOG_MEMBER(escalation, get_escalation_options)
      LOG_MEMBER(escalation, get_escalation_period)
      LOG_MEMBER(escalation, get_first_notification)
      LOG_MEMBER(escalation, get_last_notification)
      LOG_MEMBER(escalation, get_notification_interval);
  return (tmp);
}

logging::temp_logger& logging::operator<<(logging::temp_logger const& left,
                                          notification::dependency const& obj) throw() {
  logging::temp_logger& tmp = const_cast<logging::temp_logger&>(left);
  tmp LOG_MEMBER(dependency, is_host_dependency)
      LOG_MEMBER(dependency, is_service_dependency)
      LOG_MEMBER(dependency, get_period)
      LOG_MEMBER(dependency, get_kind)
      LOG_MEMBER(dependency, get_execution_failure_options)
      LOG_MEMBER(dependency, get_inherits_parent)
      LOG_MEMBER(dependency, get_notification_failure_options);
  return (tmp);
}

logging::temp_logger& logging::operator<<(logging::temp_logger const& left,
                                          notification::contact const& obj) throw() {
  logging::temp_logger& tmp = const_cast<logging::temp_logger&>(left);
  tmp LOG_MEMBER(contact, get_alias)
      LOG_MEMBER(contact, get_address)
      LOG_MEMBER(contact, get_customvariables)
      LOG_MEMBER(contact, get_can_submit_commands)
      LOG_MEMBER(contact, get_name)
      LOG_MEMBER(contact, get_email)
      LOG_MEMBER(contact, get_host_notifications_enabled)
      LOG_MEMBER(contact, get_host_notification_options)
      LOG_MEMBER(contact, get_host_notification_period)
      LOG_MEMBER(contact, get_retain_nonstatus_information)
      LOG_MEMBER(contact, get_retain_status_information)
      LOG_MEMBER(contact, get_pager)
      LOG_MEMBER(contact, get_service_notification_options)
      LOG_MEMBER(contact, get_service_notification_period)
      LOG_MEMBER(contact, get_service_notifications_enabled);
  return (tmp);
}

logging::temp_logger& logging::operator<<(logging::temp_logger const& left,
                                          notification::timeperiod const& obj) throw() {
  // TODO: Exceptions, timeranges.
  logging::temp_logger& tmp = const_cast<logging::temp_logger&>(left);
  tmp LOG_MEMBER(timeperiod, get_alias)
      LOG_MEMBER(timeperiod, get_name)
      LOG_MEMBER(timeperiod, get_timezone);
  return (tmp);
}

logging::temp_logger& logging::operator<<(logging::temp_logger const& left,
                                          notification::node const& obj) throw() {
  logging::temp_logger& tmp = const_cast<logging::temp_logger&>(left);
  tmp LOG_MEMBER(node, get_notification_number)
      LOG_MEMBER(node, get_last_notification_time)
      LOG_MEMBER(node, get_hard_state)
      LOG_MEMBER(node, get_soft_state)
      LOG_MEMBER(node, get_node_id);
  return (tmp);
}

logging::temp_logger& logging::operator<<(logging::temp_logger const& left,
                                          notification::acknowledgement const& obj) throw() {
  logging::temp_logger& tmp = const_cast<logging::temp_logger&>(left);
  tmp LOG_MEMBER(acknowledgement, get_type)
      LOG_MEMBER(acknowledgement, get_acknowledgement_type)
      LOG_MEMBER(acknowledgement, get_host_id)
      LOG_MEMBER(acknowledgement, get_service_id);
  return (tmp);
}
