/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <absl/strings/str_cat.h>

#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon::engine;
using namespace com::centreon::common::timeperiods;
using namespace com::centreon::engine::configuration::applier;

/******************************************************************/
/********************** MACRO GRAB FUNCTIONS **********************/
/******************************************************************/

/* grab macros that are specific to a particular contact */
int grab_contact_macros_r(nagios_macros* mac, contact* cntct) {
  /* clear contact-related macros */
  clear_contact_macros_r(mac);
  clear_contactgroup_macros_r(mac);

  /* save pointer to contact for later */
  mac->contact_ptr = cntct;
  mac->contactgroup_ptr = nullptr;

  if (cntct == nullptr)
    return ERROR;

  /* save pointer to first/primary contactgroup for later */
  if (!cntct->get_parent_groups().empty())
    mac->contactgroup_ptr = cntct->get_parent_groups().begin()->second;
  return OK;
}

/******************************************************************/
/******************* MACRO GENERATION FUNCTIONS *******************/
/******************************************************************/

/* calculates the value of a custom macro */
int grab_custom_macro_value_r(nagios_macros* mac,
                              const std::string_view& macro_name,
                              const std::string_view& arg1,
                              const std::string_view& arg2,
                              std::string& output) {
  std::string temp_buffer;
  int result = OK;

  /***** CUSTOM HOST MACRO *****/
  if (macro_name.rfind("_HOST", 0) == 0) {
    host* temp_host(nullptr);
    /* a standard host macro */
    if (arg2.empty()) {
      /* find the host for on-demand macros */
      if (!arg1.empty()) {
        host_map::const_iterator it = host::hosts.find(arg1);
        if (it != host::hosts.end())
          temp_host = it->second.get();

        if (temp_host == nullptr)
          return ERROR;
      }
      /* else use saved host pointer */
      else if ((temp_host = mac->host_ptr) == nullptr)
        return ERROR;

      /* get the host macro value */
      result = grab_custom_object_macro_r(mac, macro_name.substr(5),
                                          temp_host->custom_variables, output);
    }
    /* a host macro with a hostgroup name and delimiter */
    else {
      hostgroup_map::const_iterator it_hg(hostgroup::hostgroups.find(arg1));
      if (it_hg == hostgroup::hostgroups.end() || it_hg->second == nullptr)
        return ERROR;

      /* concatenate macro values for all hostgroup members */
      for (host_map_unsafe::iterator it{it_hg->second->members.begin()},
           end{it_hg->second->members.end()};
           it != end; ++it) {
        if (!it->second)
          continue;

        /* get the macro value for this host */
        grab_custom_macro_value_r(mac, macro_name, it->first, "", temp_buffer);

        if (temp_buffer.empty())
          continue;

        /* add macro value to already running macro */
        if (output.empty())
          output = temp_buffer;
        else {
          output.append(arg2);
          output.append(temp_buffer);
        }
        temp_buffer = "";
      }
    }
  }
  /***** CUSTOM SERVICE MACRO *****/
  else if (macro_name.rfind("_SERVICE", 0) == 0) {
    com::centreon::engine::service* temp_service(nullptr);

    /* use saved service pointer */
    if (arg1.empty() && arg2.empty()) {
      if ((temp_service = mac->service_ptr) == nullptr)
        return ERROR;

      /* get the service macro value */
      result = grab_custom_object_macro_r(
          mac, macro_name.substr(8), temp_service->custom_variables, output);
    }
    /* else and ondemand macro... */
    else {
      /* if first arg is blank, it means use the current host name */
      std::string_view host_name = arg1;
      if (host_name.empty()) {
        if (mac->host_ptr == nullptr)
          return ERROR;
        host_name = mac->host_ptr->name();
      }

      service_map::const_iterator found =
          service::services.find(std::make_pair(host_name, arg2));

      if (found != service::services.end() && found->second) {
        /* get the service macro value */
        result = grab_custom_object_macro_r(
            mac, macro_name.substr(8), found->second->custom_variables, output);
      }
      /* else we have a service macro with a servicegroup name and a
         delimiter... */
      else if (arg1.empty())
        return ERROR;
      else {
        servicegroup_map::const_iterator sg_it{
            servicegroup::servicegroups.find(arg1)};
        if (sg_it == servicegroup::servicegroups.end() || !sg_it->second)
          return ERROR;

        std::shared_ptr<servicegroup> const& temp_servicegroup{sg_it->second};

        /* concatenate macro values for all servicegroup members */
        for (service_map_unsafe::iterator
                 it{temp_servicegroup->members.begin()},
             end{temp_servicegroup->members.end()};
             it != end; ++it) {
          if ((temp_service = it->second) == nullptr)
            continue;

          /* get the macro value for this service */
          grab_custom_macro_value_r(mac, macro_name,
                                    temp_service->get_hostname(),
                                    temp_service->description(), temp_buffer);

          if (temp_buffer.empty())
            continue;

          /* add macro value to already running macro */
          if (output.empty())
            output = temp_buffer;
          else {
            output.append(arg2);
            output.append(temp_buffer);
          }
          temp_buffer = "";
        }
      }
    }
  }
  /***** CUSTOM CONTACT VARIABLE *****/
  else if (macro_name.rfind("_CONTACT", 0) == 0) {
    contact* temp_contact(nullptr);

    /* a standard contact macro */
    if (arg2.empty()) {
      /* find the contact for on-demand macros */
      if (!arg1.empty()) {
        contact_map::const_iterator it{contact::contacts.find(arg1)};
        if (it == contact::contacts.end())
          return ERROR;
        else
          temp_contact = it->second.get();
      }
      /* else use saved contact pointer */
      else if ((temp_contact = mac->contact_ptr) == nullptr)
        return ERROR;

      /* get the contact macro value */
      result = grab_custom_object_macro_r(mac, macro_name.substr(8),
                                          temp_contact->get_custom_variables(),
                                          output);
    }
    /* a contact macro with a contactgroup name and delimiter */
    else {
      contactgroup_map::iterator cg_it = contactgroup::contactgroups.find(arg1);
      if (cg_it == contactgroup::contactgroups.end() || !cg_it->second)
        return (ERROR);

      /* concatenate macro values for all contactgroup members */
      for (contact_map::const_iterator
               it = cg_it->second->get_members().begin(),
               end = cg_it->second->get_members().end();
           it != end; ++it) {
        if (!it->second)
          continue;

        /* get the macro value for this contact */
        grab_custom_macro_value_r(
            mac, macro_name, it->second->get_name().c_str(), "", temp_buffer);

        if (temp_buffer.empty())
          continue;

        /* add macro value to already running macro */
        if (output.empty())
          output = temp_buffer;
        else {
          output.append(arg2);
          output.append(temp_buffer);
        }
        temp_buffer = "";
      }
    }
  } else
    return ERROR;
  return result;
}

/* calculates a date/time macro */
int grab_datetime_macro_r(nagios_macros* mac,
                          int macro_type,
                          const std::string_view& arg1,
                          const std::string_view& arg2,
                          std::string& output) {
  time_t current_time = 0L;
  timeperiod* temp_timeperiod = nullptr;
  time_t test_time = 0L;
  time_t next_valid_time = 0L;
  timeperiod_map::const_iterator it;
  char tmp_date[MAX_DATETIME_LENGTH];

  (void)mac;

  /* get the current time */
  time(&current_time);

  /* parse args, do prep work */
  switch (macro_type) {
    case MACRO_ISVALIDTIME:
    case MACRO_NEXTVALIDTIME:
      /* find the timeperiod */
      temp_timeperiod = nullptr;
      it = ::timeperiods.find(arg1);

      if (it != ::timeperiods.end())
        temp_timeperiod = it->second.get();

      if (temp_timeperiod == nullptr)
        return ERROR;

      /* what timestamp should we use? */
      if (!arg2.empty()) {
        if (!absl::SimpleAtoi(arg2, &test_time))
          test_time = current_time;
      } else
        test_time = current_time;
      break;

    default:
      break;
  }

  /* calculate the value */
  switch (macro_type) {
    case MACRO_LONGDATETIME:
      get_datetime_string(&current_time, tmp_date, MAX_DATETIME_LENGTH,
                          LONG_DATE_TIME);
      output = tmp_date;
      break;

    case MACRO_SHORTDATETIME:
      get_datetime_string(&current_time, tmp_date, MAX_DATETIME_LENGTH,
                          SHORT_DATE_TIME);
      output = tmp_date;
      break;

    case MACRO_DATE:
      get_datetime_string(&current_time, tmp_date, MAX_DATETIME_LENGTH,
                          SHORT_DATE);
      output = tmp_date;
      break;

    case MACRO_TIME:
      get_datetime_string(&current_time, tmp_date, MAX_DATETIME_LENGTH,
                          SHORT_TIME);
      output = tmp_date;
      break;

    case MACRO_TIMET:
      output = fmt::to_string(current_time);
      break;

    case MACRO_ISVALIDTIME:
      output = std::to_string(
          !temp_timeperiod->check_time_against_period(test_time));
      break;

    case MACRO_NEXTVALIDTIME:
      next_valid_time = temp_timeperiod->get_next_valid_time(test_time);
      if (next_valid_time == test_time &&
          !temp_timeperiod->check_time_against_period(test_time))
        next_valid_time = (time_t)0L;
      get_datetime_string(&next_valid_time, tmp_date, MAX_DATETIME_LENGTH,
                          SHORT_TIME);
      output = tmp_date;
      break;

    default:
      return ERROR;
  }
  return OK;
}

/* computes a hostgroup macro */
int grab_standard_hostgroup_macro_r(nagios_macros* mac,
                                    int macro_type,
                                    hostgroup* temp_hostgroup,
                                    std::string& output) {
  std::string temp_buffer;
  if (temp_hostgroup == nullptr)
    return ERROR;

  /* get the macro value */
  switch (macro_type) {
    case MACRO_HOSTGROUPNAME:
      output = temp_hostgroup->get_group_name();
      break;

    case MACRO_HOSTGROUPALIAS:
      if (!temp_hostgroup->get_alias().empty())
        output = temp_hostgroup->get_alias();
      break;

    case MACRO_HOSTGROUPMEMBERS:
      /* now fill in the string with the member names */
      for (host_map_unsafe::const_iterator it{temp_hostgroup->members.begin()},
           end{temp_hostgroup->members.end()};
           it != end; ++it) {
        if (it->first.empty())
          continue;
        if (output.empty()) /* If our buffer didn't contain anything, we
                                    just need to write "%s,%s" */
          output.append(it->first);
        else
          output += "," + it->first;
      }
      break;

    case MACRO_HOSTGROUPACTIONURL:
      if (!temp_hostgroup->get_action_url().empty())
        output = temp_hostgroup->get_action_url();
      break;

    case MACRO_HOSTGROUPNOTESURL:
      if (!temp_hostgroup->get_notes_url().empty())
        output = temp_hostgroup->get_notes_url();
      break;

    case MACRO_HOSTGROUPNOTES:
      if (!temp_hostgroup->get_notes().empty())
        output = temp_hostgroup->get_notes();
      break;

    default:
      macros_logger->trace("UNHANDLED HOSTGROUP MACRO #{}! THIS IS A BUG!",
                           macro_type);
      return ERROR;
  }

  /* post-processing */
  /* notes, notes URL and action URL macros may themselves contain macros, so
   * process them... */
  switch (macro_type) {
    case MACRO_HOSTGROUPACTIONURL:
    case MACRO_HOSTGROUPNOTESURL:
      process_macros_r(mac, output, temp_buffer, 0);
      output = temp_buffer;
      break;

    case MACRO_HOSTGROUPNOTES:
      process_macros_r(mac, output, temp_buffer, 0);
      output = temp_buffer;
      break;

    default:
      break;
  }

  return OK;
}

/* computes a servicegroup macro */
int grab_standard_servicegroup_macro_r(nagios_macros* mac,
                                       int macro_type,
                                       servicegroup* temp_servicegroup,
                                       std::string& output) {
  std::string temp_buffer;

  if (temp_servicegroup == nullptr)
    return ERROR;

  /* get the macro value */
  switch (macro_type) {
    case MACRO_SERVICEGROUPNAME:
      output = temp_servicegroup->get_group_name();
      break;

    case MACRO_SERVICEGROUPALIAS:
      if (!temp_servicegroup->get_alias().empty())
        output = temp_servicegroup->get_alias();
      break;

    case MACRO_SERVICEGROUPMEMBERS:
      /* fill the string with the group members */
      for (service_map_unsafe::iterator it{temp_servicegroup->members.begin()},
           end{temp_servicegroup->members.end()};
           it != end; ++it) {
        if (it->first.first.empty() || it->first.second.empty())
          continue;
        if (temp_buffer.empty()) /* If our buffer didn't contain anything, we
                                    just need to write "%s,%s" */
          temp_buffer = it->first.first + "," + it->first.second;
        else /* Now we need to write ",%s,%s" */
          absl::StrAppend(&temp_buffer, ",", it->first.first, ",",
                          it->first.second);
      }
      output = temp_buffer;
      break;
    case MACRO_SERVICEGROUPACTIONURL:
      if (!temp_servicegroup->get_action_url().empty())
        output = temp_servicegroup->get_action_url();
      break;

    case MACRO_SERVICEGROUPNOTESURL:
      if (!temp_servicegroup->get_notes_url().empty())
        output = temp_servicegroup->get_notes_url();
      break;

    case MACRO_SERVICEGROUPNOTES:
      if (!temp_servicegroup->get_notes().empty())
        output = temp_servicegroup->get_notes();
      break;

    default:
      macros_logger->trace("UNHANDLED SERVICEGROUP MACRO #{}! THIS IS A BUG!",
                           macro_type);
      return ERROR;
  }

  /* post-processing */
  /* notes, notes URL and action URL macros may themselves contain macros, so
   * process them... */
  switch (macro_type) {
    case MACRO_SERVICEGROUPACTIONURL:
    case MACRO_SERVICEGROUPNOTESURL:
      process_macros_r(mac, output, temp_buffer, 0);
      output = temp_buffer;
      break;

    case MACRO_SERVICEGROUPNOTES:
      process_macros_r(mac, output, temp_buffer, 0);
      output = temp_buffer;
      break;

    default:
      break;
  }
  return OK;
}

/* computes a contact macro */
int grab_standard_contact_macro_r(nagios_macros* mac,
                                  int macro_type,
                                  contact* temp_contact,
                                  std::string& output) {
  (void)mac;

  if (temp_contact == nullptr)
    return ERROR;

  /* get the macro value */
  switch (macro_type) {
    case MACRO_CONTACTNAME:
      output = temp_contact->get_name();
      break;

    case MACRO_CONTACTALIAS:
      output = temp_contact->get_alias();
      break;

    case MACRO_CONTACTEMAIL:
      if (!temp_contact->get_email().empty())
        output = temp_contact->get_email();
      break;

    case MACRO_CONTACTPAGER:
      if (!temp_contact->get_pager().empty())
        output = temp_contact->get_pager();
      break;

    case MACRO_CONTACTGROUPNAMES: {
      std::string buf;
      /* get the contactgroup names */
      /* find all contactgroups this contact is a member of */
      for (contactgroup_map_unsafe::const_iterator
               it{temp_contact->get_parent_groups().begin()},
           end{temp_contact->get_parent_groups().end()};
           it != end; ++it) {
        if (!it->second)
          continue;

        if (!buf.empty())
          buf.append(",");
        buf.append(it->first);
      }
      if (!buf.empty())
        output = buf;
    } break;

    case MACRO_CONTACTGROUPNAME: {
      /* get the contactgroup names */
      /* find contactgroup this contact is a member of */
      if (!temp_contact->get_parent_groups().empty()) {
        output = temp_contact->get_parent_groups().begin()->first;
      }
    } break;

    case MACRO_CONTACTTIMEZONE: {
      output = temp_contact->get_timezone();
    } break;

    default:
      macros_logger->trace("UNHANDLED CONTACT MACRO #{}! THIS IS A BUG!",
                           macro_type);
      return ERROR;
  }
  return OK;
}

/* computes a contact address macro */
int grab_contact_address_macro(unsigned int macro_num,
                               contact* temp_contact,
                               std::string& output) {
  if (macro_num >= MAX_CONTACT_ADDRESSES)
    return ERROR;

  if (temp_contact == nullptr)
    return ERROR;

  /* get the macro, the contact may have less than MAX_CONTACT_ADDRESSES
   * addresses */
  if (macro_num < temp_contact->get_addresses().size())
    output = temp_contact->get_address(macro_num);
  return OK;
}

/* computes a contactgroup macro */
int grab_standard_contactgroup_macro(
    int macro_type,
    std::shared_ptr<com::centreon::engine::contactgroup> temp_contactgroup,
    std::string& output) {
  if (temp_contactgroup == nullptr)
    return ERROR;

  /* get the macro value */
  switch (macro_type) {
    case MACRO_CONTACTGROUPNAME:
      output = temp_contactgroup->get_name();
      break;

    case MACRO_CONTACTGROUPALIAS:
      if (!temp_contactgroup->get_alias().empty())
        output = temp_contactgroup->get_alias();
      break;

    case MACRO_CONTACTGROUPMEMBERS:
      /* get the member list */
      for (contact_map::const_iterator
               it = temp_contactgroup->get_members().begin(),
               end = temp_contactgroup->get_members().end();
           it != end; ++it) {
        if (it->second->get_name().empty())
          continue;
        if (output.empty())
          output = it->first;
        else {
          output.append(",");
          output.append(it->first);
        }
      }
      break;

    default:
      macros_logger->trace("UNHANDLED CONTACTGROUP MACRO #{}! THIS IS A BUG!",
                           macro_type);
      return ERROR;
  }
  return OK;
}

/* computes a custom object macro */
int grab_custom_object_macro_r(nagios_macros* mac,
                               const std::string_view& macro_name,
                               map_customvar const& vars,
                               std::string& output) {
  int result = ERROR;

  (void)mac;

  if (macro_name.empty() || vars.empty())
    return ERROR;

  /* get the custom variable */
  for (auto const& cv : vars) {
    if (macro_name == cv.first) {
      output = cv.second.value();
      result = OK;
      break;
    }
  }
  return result;
}
/******************************************************************/
/********************* MACRO STRING FUNCTIONS *********************/
/******************************************************************/

/* cleans illegal characters in macros before output */
std::string clean_macro_chars(std::string const& macro, int options) {
  std::string retval(macro);

  int len(retval.size());

  /* strip illegal characters out of retval */
  if (options & STRIP_ILLEGAL_MACRO_CHARS) {
    int y(0);
    for (int x(0); x < len; x++) {
      /*ch=(int)retval[x]; */
      /* allow non-ASCII characters (Japanese, etc) */
      int ch(retval[x] & 0xff);

      /* illegal ASCII characters */
      if (ch < 32 || ch == 127)
        continue;

      /* illegal user-specified characters */
      if (pb_indexed_config.state().illegal_output_chars().find(ch) ==
          std::string::npos)
        retval[y++] = retval[x];
    }

    retval.resize(y);
  }
  return retval;
}

/******************************************************************/
/***************** MACRO INITIALIZATION FUNCTIONS *****************/
/******************************************************************/

/* initializes global macros */
int init_macros() {
  /*
   * non-volatile macros are free()'d when they're set.
   * We must do this in order to not lose the constant
   * ones when we get SIGHUP or a RESTART_PROGRAM event
   * from the command fifo. Otherwise a memset() would
   * have been better.
   */
  clear_volatile_macros_r(get_global_macros());

  /* backwards compatibility hack */
  return OK;
}

/******************************************************************/
/********************* MACRO CLEANUP FUNCTIONS ********************/
/******************************************************************/

/* clear argv macros - used in commands */
int clear_argv_macros_r(nagios_macros* mac) {
  unsigned int x = 0;

  /* command argument macros */
  for (x = 0; x < MAX_COMMAND_ARGUMENTS; x++) {
    mac->argv[x] = "";
  }
  return OK;
}

/* clear all macros that are not "constant" (i.e. they change throughout the
 * course of monitoring) */
int clear_volatile_macros_r(nagios_macros* mac) {
  unsigned int x = 0;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {
      case MACRO_ADMINEMAIL:
      case MACRO_ADMINPAGER:
      case MACRO_MAINCONFIGFILE:
      case MACRO_STATUSDATAFILE:
      case MACRO_RETENTIONDATAFILE:
      case MACRO_TEMPFILE:
      case MACRO_LOGFILE:
      case MACRO_RESOURCEFILE:
      case MACRO_COMMANDFILE:
      case MACRO_HOSTPERFDATAFILE:
      case MACRO_SERVICEPERFDATAFILE:
      case MACRO_PROCESSSTARTTIME:
      case MACRO_TEMPPATH:
      case MACRO_EVENTSTARTTIME:
      case MACRO_POLLERNAME:
      case MACRO_POLLERID:
        /* these don't change during the course of monitoring, so no need to
         * free them */
        break;

      default:
        mac->x[x] = "";
        break;
    }
  }

  /* contact address macros */
  for (x = 0; x < MAX_CONTACT_ADDRESSES; x++) {
    mac->contactaddress[x] = "";
  }

  /* clear macro pointers */
  mac->host_ptr = nullptr;
  mac->hostgroup_ptr = nullptr;
  mac->service_ptr = nullptr;
  mac->servicegroup_ptr = nullptr;
  mac->contact_ptr = nullptr;
  mac->contactgroup_ptr = nullptr;

  /* clear on-demand macro */
  mac->ondemand = "";

  /* clear ARGx macros */
  clear_argv_macros_r(mac);

  /* clear custom host variables */
  mac->custom_host_vars.clear();

  /* clear custom service variables */
  mac->custom_service_vars.clear();

  /* clear custom contact variables */
  mac->custom_contact_vars.clear();

  return OK;
}

/* clear contact macros */
int clear_contact_macros_r(nagios_macros* mac) {
  unsigned int x;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {
      case MACRO_CONTACTNAME:
      case MACRO_CONTACTALIAS:
      case MACRO_CONTACTEMAIL:
      case MACRO_CONTACTPAGER:
      case MACRO_CONTACTGROUPNAMES:
        mac->x[x] = "";
        break;

      default:
        break;
    }
  }

  /* clear contact addresses */
  for (x = 0; x < MAX_CONTACT_ADDRESSES; x++) {
    mac->contactaddress[x] = "";
  }

  /* clear custom contact variables */
  mac->custom_contact_vars.clear();

  /* clear pointers */
  mac->contact_ptr = nullptr;

  return OK;
}

/* clear contactgroup macros */
int clear_contactgroup_macros_r(nagios_macros* mac) {
  unsigned int x;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {
      case MACRO_CONTACTGROUPNAME:
      case MACRO_CONTACTGROUPALIAS:
      case MACRO_CONTACTGROUPMEMBERS:
        mac->x[x] = "";
        break;

      default:
        break;
    }
  }

  /* clear pointers */
  mac->contactgroup_ptr = nullptr;

  return OK;
}

/* clear summary macros */
int clear_summary_macros_r(nagios_macros* mac) {
  unsigned int x;

  for (x = MACRO_TOTALHOSTSUP; x <= MACRO_TOTALSERVICEPROBLEMSUNHANDLED; x++) {
    mac->x[x] = "";
  }

  return OK;
}
