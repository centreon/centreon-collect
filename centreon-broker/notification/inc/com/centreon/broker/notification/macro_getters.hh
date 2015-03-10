/*
** Copyright 2011-2014 Merethis
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

#ifndef CCB_NOTIFICATION_MACRO_GETTERS_HH
#  define CCB_NOTIFICATION_MACRO_GETTERS_HH

#  include <QString>
#  include <QHash>
#  include <QList>
#  include <string>
#  include <sstream>
#  include <iomanip>
#  include "com/centreon/broker/namespace.hh"
#  include "com/centreon/broker/notification/macro_context.hh"
#  include "com/centreon/broker/notification/utilities/get_datetime_string.hh"
#  include "com/centreon/broker/exceptions/msg.hh"

CCB_BEGIN()

namespace        notification {

  /**
   *  Set the precision of a stringstream.
   *
   *  @tparam precision  The precision.
   *
   *  @param[out] ost    The stringstream to modify.
   */
  template <int precision>
  void set_precision(std::ostringstream& ost) {
     ost << std::fixed << std::setprecision(precision);
  }

  /**
   *  @brief Does not set the precision of a stringstream.
   *
   *  Specialization of set_precision for precision = 0;
   *
   *  @param[out] ost    The stringstream to modify.
   */
  template <> void inline set_precision<0>(std::ostringstream& ost) {
    (void)ost;
  }

  /**
   *  Write a type into string.
   *
   *  @tparam T           The type.
   *  @tparam precision   The precision one wants, 0 for none.
   *
   *  @param[in] value    The value.
   *
   *  @return  The value of the macro.
   */
  template <typename T, int precision>
  std::string to_string(T const& value) {
    std::ostringstream ost;
    set_precision<precision>(ost);
    ost << value;
    return (ost.str());
  }

  /**
   *  @brief Write a type into a string.
   *
   *  Specialization for std::string and 0 precision.
   *
   *  @param[in] value  The value of the string.
   *
   *  @return  The value of the macro.
   */
  template <> std::string inline to_string <std::string, 0>(
                                            std::string const& value) {
    return (value);
  }

  /**
   *  @brief Write a type into a string.
   *
   *  Specialization for QString and 0 precision.
   *
   *  @param[in] value  The value of the string.
   *
   *  @return  The value of the macro.
   */
  template <> std::string inline to_string <QString, 0>(
                                            QString const& value) {
    return (value.toStdString());
  }

  /**
   *  Get the member of a host as a string.
   *
   *  @tparam T           The type of the host object.
   *  @tparam U           The type of the member.
   *  @tparam Member      The member.
   *  @tparam precision   The precision of the string, 0 for none.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <typename T, typename U, U (T::* member), int precision>
    std::string get_host_member_as_string(
                  macro_context const& context) {
    return (to_string<U, precision>(
              context.get_cache().get_host(context.get_id())
                                 .get_node().*member));
  }

  /**
   *  Get the member of a service as a string.
   *
   *  @tparam T           The type of the service object.
   *  @tparam U           The type of the member.
   *  @tparam Member      The member.
   *  @tparam precision   The precision of the string, 0 for none.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <typename T, typename U, U (T::* member), int precision>
    std::string get_service_member_as_string(
                  macro_context const& context) {
    return (to_string<U, precision>(
              context.get_cache().get_service(context.get_id())
                                 .get_node().*member));
  }

  /**
   *  Get the member of a host status as a string.
   *
   *  @tparam T           The type of the host status object.
   *  @tparam U           The type of the member.
   *  @tparam Member      The member.
   *  @tparam precision   The precision of the string, 0 for none.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <typename T, typename U, U (T::* member), int precision>
    std::string get_host_status_member_as_string(
                  macro_context const& context) {
      return (to_string<U, precision>(
                context.get_cache().get_host(context.get_id())
                                   .get_status().*member));
  }

  /**
   *  Get the member of a service status as a string.
   *
   *  @tparam T           The type of the service status object.
   *  @tparam U           The type of the member.
   *  @tparam Member      The member.
   *  @tparam precision   The precision of the string, 0 for none.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <typename T, typename U, U (T::* member), int precision>
    std::string get_service_status_member_as_string(
                  macro_context const& context) {
      return (to_string<U, precision>(
                context.get_cache().get_service(context.get_id())
                                   .get_status().*member));
  }

  /**
   *  Get the member of a previous host status as a string.
   *
   *  @tparam T           The type of the previous host status object.
   *  @tparam U           The type of the member.
   *  @tparam Member      The member.
   *  @tparam precision   The precision of the string, 0 for none.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <typename T, typename U, U (T::* member), int precision>
    std::string get_host_prevstatus_member_as_string(
                  macro_context const& context) {
      return (to_string<U, precision>(
                context.get_cache().get_host(context.get_id())
                                   .get_prev_status().*member));
  }

  /**
   *  Get the member of a previous service status as a string.
   *
   *  @tparam T           The type of the previous service status object.
   *  @tparam U           The type of the member.
   *  @tparam Member      The member.
   *  @tparam precision   The precision of the string, 0 for none.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <typename T, typename U, U (T::* member), int precision>
    std::string get_service_prevstatus_member_as_string(
                  macro_context const& context) {
      return (to_string<U, precision>(
                context.get_cache().get_service(context.get_id())
                                   .get_prev_status().*member));
  }

  /**
   *  Get the groups of a host.
   *
   *  @tparam get_all     True if we want all the groups, false if only one.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <bool get_all>
  std::string get_host_groups(
                 macro_context const& context) {return ("");}

  template <> std::string get_host_groups<true>(
                            macro_context const& context);

  template <> std::string get_host_groups<false>(
                            macro_context const& context);

  /**
   *  Get the groups of a service.
   *
   *  @tparam get_all     True if we want all the groups, false if only one.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <bool get_all>
  std::string get_service_groups(
                 macro_context const& context) {return ("");}

  template <> std::string get_service_groups<true>(
                            macro_context const& context);

  template <> std::string get_service_groups<false>(
                            macro_context const& context);

  /**
   *  Get the output of a host.
   *
   *  @tparam get_long_output  True if we want the long output, false if we want the short one.
   *
   *  @param[in] context       The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <bool get_long_output>
  std::string get_host_output(
                macro_context const& context) {return ("");}

  template <> std::string get_host_output<false>(
                            macro_context const& context);

  template <> std::string get_host_output<true>(
                            macro_context const& context);

  /**
   *  Get the output of a service.
   *
   *  @tparam get_long_output  True if we want the long output, false if we want the short one.
   *
   *  @param[in] context       The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <bool get_long_output>
  std::string get_service_output(
                macro_context const& context) {return ("");}

  template <> std::string get_service_output<false>(
                            macro_context const& context);

  template <> std::string get_service_output<true>(
                            macro_context const& context);

  /**
   *  Get the date or time string.
   *
   *  @tparam date_type   The type of the date.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <int date_type>
  std::string get_datetime_string(
                macro_context const& context) {
    return (utilities::get_datetime_string(
                 ::time(NULL),
                 48,
                 date_type,
                 context.get_state().get_date_format()));
  }

  /**
   *  Get the total number of services associated with the host.
   *
   *  @tparam service_state  The state of the services we want to count: -1 for all.
   *
   *  @param[in] context     The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <int service_state>
  std::string get_total_host_services(
                macro_context const& context) {
    QList<objects::node::ptr> services =
      context.get_state().get_all_services_of_host(context.get_id().to_host());
    if (service_state != -1) {
      size_t count = 0;
      for (QList<objects::node::ptr>::iterator it(services.begin()),
                                               end(services.end());
           it != end;++it)
        if ((*it)->get_hard_state() == objects::node_state(service_state))
          ++count;
      return (to_string<size_t, 0>(count));
    }
    else
      return (to_string<int, 0>(services.count()));
  }

  /**
   *  Get the total number of hosts with a given state.
   *
   *  @tparam host_state  The state of the hosts we want to count. -1 for all that are not up.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <int host_state>
  std::string get_total_hosts(
                macro_context const& context) {
    return (to_string<int, 0>(
              context.get_state().get_all_hosts_in_state(host_state).size()));
  }

  /**
   *  Get the total number of services with a given state.
   *
   *  @tparam service_state  The state of the services we want to count. -1 for all that are not ok.
   *
   *  @param[in] context     The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <int service_state>
  std::string get_total_services(
                macro_context const& context) {
    return (to_string<int, 0>(
              context.get_state().get_all_services_in_state(service_state).size()));
  }

  /**
   *  Get the total number of hosts unhandled with a given state.
   *
   *  @tparam host_state  The state of the hosts we want to count. -1 for all that are not up.
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <int host_state>
  std::string get_total_hosts_unhandled(
                macro_context const& context) {
    size_t count = 0;
    state const& st = context.get_state();
    QList<objects::node::ptr> list = st.get_all_hosts_in_state(host_state);
    for (QList<objects::node::ptr>::const_iterator it = list.begin(),
                                                   end = list.end();
         it != end;
         ++it) {
      if (!st.has_node_been_acknowledged((*it)->get_node_id()) &&
            !st.is_node_in_downtime((*it)->get_node_id()))
        ++count;
    }
    return (to_string<size_t, 0>(count));
  }

  /**
   *  Get the total number of services unhandled with a given state.
   *
   *  @tparam service_state  The state of the services we want to count. -1 for all that are not ok.
   *
   *  @param[in] context     The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <int service_state>
  std::string get_total_services_unhandled(
                macro_context const& context) {
    size_t count = 0;
    state const& st = context.get_state();
    QList<objects::node::ptr> list = st.get_all_services_in_state(service_state);
    for (QList<objects::node::ptr>::const_iterator it = list.begin(),
                                                   end = list.end();
         it != end;
         ++it) {
      if (!st.has_node_been_acknowledged((*it)->get_node_id()) &&
            !st.is_node_in_downtime((*it)->get_node_id()))
        ++count;
    }
    return (to_string<size_t, 0>(count));
  }

  /**
   *  Get the group alias of a node.
   *
   *  @tparam is_host     Is the node an host ?
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <bool is_host>
  std::string get_group_alias(
                macro_context const& context) {
    objects::nodegroup::ptr ngr;
    state const& st = context.get_state();
    node_cache const& cache = context.get_cache();
    if (is_host) {
      std::map<std::string, neb::host_group_member> map =
        cache.get_host(context.get_id()).get_groups();
      if (map.empty())
        return ("");
      ngr = st.get_nodegroup_by_name(map.begin()->first);
    }
    else {
      std::map<std::string, neb::service_group_member> map =
        cache.get_service(context.get_id()).get_groups();
      if (map.empty())
        return ("");
      ngr = st.get_nodegroup_by_name(map.begin()->first);
    }

    if (!ngr)
      throw (exceptions::msg()
             << "notification: macro: could not get a group");
    return (ngr->get_alias());
  }

  /**
   *  Get the group members of a node.
   *
   *  @tparam is_host     Is the node an host ?
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <bool is_host>
  std::string get_group_members(
                macro_context const& context) {
    std::vector<std::string> members;
    node_cache const& cache = context.get_cache();
    if (is_host) {
      std::map<std::string, neb::host_group_member> map =
        cache.get_host(context.get_id()).get_groups();
      if (map.empty())
        return ("");
      members = cache.get_all_node_contained_in(map.begin()->first, true);
    }
    else {
      std::map<std::string, neb::service_group_member> map =
        cache.get_service(context.get_id()).get_groups();
      if (map.empty())
        return ("");
      members = cache.get_all_node_contained_in(map.begin()->first, false);
    }
    std::string res;
    for (std::vector<std::string>::const_iterator it(members.begin()),
                                                  end(members.end());
         it != end;
         ++it) {
      if (!res.empty())
        res.append(", ");
      res.append(*it);
    }
    return (res);
  }

  /**
   *  Get the member of a contact as a string.
   *
   *  @tparam U          The type of the member.
   *  @tparam Member     The member.
   *  @tparam precision  The precision of the string, 0 for none.
   *
   *  @param[in] context The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <typename U, U (objects::contact::* member)() const, int precision>
  std::string get_contact_member(
                macro_context const& context) {
    return (to_string<U, precision>((context.get_contact().*member)()));
  }

  /**
   *  Get the member of an action as a string.
   *
   *  @tparam U          The type of the member.
   *  @tparam Member     The member.
   *  @tparam precision  The precision of the string, 0 for none.
   *
   *  @param[in] context The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <typename U, U (action::* member)() const, int precision>
  std::string get_action_member(
                macro_context const& context) {
    return (to_string<U, precision>((context.get_action().*member)()));
  }

  /**
   *  Get the address of the contact.
   *
   *  @tparam number           The address number (1 to 6).
   *
   *  @param[in] context  The context from where the macro is being executed.
   *
   *  @return  The value of the macro.
   */
  template <int number>
  std::string get_address_of_contact(
                macro_context const& context) {
    return (context.get_contact().get_address().at(number - 1));
  }

  // Static, non template getters.
  std::string   get_host_state(
                  macro_context const& context);

  std::string  get_service_state(
                 macro_context const& context);

  std::string   get_last_host_state(
                  macro_context const& context);

  std::string   get_last_service_state(
                  macro_context const& context);

  std::string   get_host_state_type(
                  macro_context const& context);

  std::string  get_service_state_type(
                 macro_context const& context);

  std::string   null_getter(
                  macro_context const& context);

  std::string   get_host_duration(
                  macro_context const& context);

  std::string  get_service_duration(
                 macro_context const& context);

  std::string   get_host_duration_sec(
                  macro_context const& context);

  std::string  get_service_duration_sec(
                 macro_context const& context);

  std::string  get_timet_string(
                 macro_context const& context);

  std::string  get_notification_type(
                 macro_context const& context);
}

CCB_END()

#endif // !CCB_NOTIFICATION_MACRO_GETTERS_HH
