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

#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
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

  /* clear macro pointers */
  mac->host_ptr = nullptr;
  mac->hostgroup_ptr = nullptr;
  mac->service_ptr = nullptr;
  mac->servicegroup_ptr = nullptr;
  mac->contact_ptr = nullptr;
  mac->contactgroup_ptr = nullptr;

  /* clear ARGx macros */
  clear_argv_macros_r(mac);

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
