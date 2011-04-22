/*
** Copyright 1999-2010 Ethan Galstad
** Copyright 2011      Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <sstream>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include "engine.hh"
#include "utils.hh"
#include "shared.hh"
#include "logging.hh"
#include "configuration.hh"
#include "macros.hh"

using namespace com::centreon::engine;

extern configuration config;

extern contact*      contact_list;
extern contactgroup* contactgroup_list;
extern host*         host_list;
extern hostgroup*    hostgroup_list;
extern service*      service_list;
extern servicegroup* servicegroup_list;
extern command*      command_list;
extern timeperiod*   timeperiod_list;

char*                macro_x_names[MACRO_X_COUNT];     /* the macro names */
char*                macro_user[MAX_USER_MACROS];      /* $USERx$ macros */

/*
 * These point to their corresponding pointer arrays in global_macros
 * AFTER macros have been initialized.
 *
 * They really only exist so that eventbroker modules that reference
 * them won't need to be re-compiled, although modules that rely
 * on their values after having run a certain command will require an
 * update
 */
char**               macro_x = NULL;

/*
 * scoped to this file to prevent (unintentional) mischief,
 * but see base/notifications.c for how to use it
 */
static nagios_macros global_macros;

nagios_macros* get_global_macros(void) {
  return (&global_macros);
}

/******************************************************************/
/************************ MACRO FUNCTIONS *************************/
/******************************************************************/

/*
 * replace macros in notification commands with their values,
 * the thread-safe version
 */
int process_macros_r(nagios_macros* mac,
		     char* input_buffer,
                     char** output_buffer,
		     int options) {
  char* temp_buffer = NULL;
  char* save_buffer = NULL;
  char* buf_ptr = NULL;
  char* delim_ptr = NULL;
  int in_macro = FALSE;
  int x = 0;
  char* selected_macro = NULL;
  char* original_macro = NULL;
  char const* cleaned_macro = NULL;
  int clean_macro = FALSE;
  int found_macro_x = FALSE;
  int result = OK;
  int clean_options = 0;
  int free_macro = FALSE;
  int macro_options = 0;

  log_debug_info(DEBUGL_FUNCTIONS, 0, "process_macros_r()\n");

  if (output_buffer == NULL)
    return (ERROR);

  *output_buffer = my_strdup("");

  if (input_buffer == NULL)
    return (ERROR);

  in_macro = FALSE;

  log_debug_info(DEBUGL_MACROS, 1, "**** BEGIN MACRO PROCESSING ***********\n");
  log_debug_info(DEBUGL_MACROS, 1, "Processing: '%s'\n", input_buffer);

  /* use a duplicate of original buffer, so we don't modify the original */
  save_buffer = buf_ptr = (input_buffer ? my_strdup(input_buffer) : NULL);

  while (buf_ptr) {
    /* save pointer to this working part of buffer */
    temp_buffer = buf_ptr;

    /* find the next delimiter - terminate preceding string and advance buffer pointer for next run */
    if ((delim_ptr = strchr(buf_ptr, '$'))) {
      delim_ptr[0] = '\x0';
      buf_ptr = (char* )delim_ptr + 1;
    }
    /* no delimiter found - we already have the last of the buffer */
    else
      buf_ptr = NULL;

    log_debug_info(DEBUGL_MACROS, 2, "  Processing part: '%s'\n", temp_buffer);

    selected_macro = NULL;
    found_macro_x = FALSE;
    clean_macro = FALSE;

    /* we're in plain text... */
    if (in_macro == FALSE) {

      /* add the plain text to the end of the already processed buffer */
      *output_buffer = resize_string(*output_buffer,
				     strlen(*output_buffer) + strlen(temp_buffer) + 1);
      strcat(*output_buffer, temp_buffer);

      log_debug_info(DEBUGL_MACROS, 2,
                     "  Not currently in macro.  Running output (%zd): '%s'\n",
                     strlen(*output_buffer), *output_buffer);
      in_macro = TRUE;
    }
    /* looks like we're in a macro, so process it... */
    else {

      /* reset clean options */
      clean_options = 0;

      /* grab the macro value */
      result = grab_macro_value(mac,
				temp_buffer,
				&selected_macro,
				&clean_options,
				&free_macro);
      log_debug_info(DEBUGL_MACROS, 2,
		     "  Processed '%s', Clean Options: %d, Free: %d\n",
		     temp_buffer, clean_options, free_macro);

      /* an error occurred - we couldn't parse the macro, so continue on */
      if (result == ERROR) {
        log_debug_info(DEBUGL_MACROS, 0,
                       " WARNING: An error occurred processing macro '%s'!\n",
                       temp_buffer);
        if (free_macro == TRUE) {
          delete[] selected_macro;
          selected_macro = NULL;
        }
      }

      /* we already have a macro... */
      if (result == OK)
        x = 0;
      /* an escaped $ is done by specifying two $$ next to each other */
      else if (!strcmp(temp_buffer, "")) {
        log_debug_info(DEBUGL_MACROS, 2,
                       "  Escaped $.  Running output (%zd): '%s'\n",
                       strlen(*output_buffer), *output_buffer);
        *output_buffer = resize_string(*output_buffer, strlen(*output_buffer) + 2);
        strcat(*output_buffer, "$");
      }
      /* a non-macro, just some user-defined string between two $s */
      else {
        log_debug_info(DEBUGL_MACROS, 2,
                       "  Non-macro.  Running output (%zd): '%s'\n",
                       strlen(*output_buffer), *output_buffer);

        /* add the plain text to the end of the already processed buffer */
        /*
         *output_buffer=(char*)realloc(*output_buffer,strlen(*output_buffer)+strlen(temp_buffer)+3);
         strcat(*output_buffer,"$");
         strcat(*output_buffer,temp_buffer);
         strcat(*output_buffer,"$");
	*/
      }

      /* insert macro */
      if (selected_macro != NULL) {
        log_debug_info(DEBUGL_MACROS, 2,
                       "  Processed '%s', Clean Options: %d, Free: %d\n",
                       temp_buffer, clean_options, free_macro);

        /* include any cleaning options passed back to us */
        macro_options = (options | clean_options);

        log_debug_info(DEBUGL_MACROS, 2,
                       "  Cleaning options: global=%d, local=%d, effective=%d\n",
                       options, clean_options, macro_options);

        /* URL encode the macro if requested - this allocates new memory */
        if (macro_options & URL_ENCODE_MACRO_CHARS) {
          original_macro = selected_macro;
          selected_macro = get_url_encoded_string(selected_macro);
          if (free_macro == TRUE) {
            delete[] original_macro;
            original_macro = NULL;
          }
          free_macro = TRUE;
        }

        /* some macros are cleaned... */
        if (clean_macro == TRUE
            || ((macro_options & STRIP_ILLEGAL_MACRO_CHARS)
                || (macro_options & ESCAPE_MACRO_CHARS))) {

          /* add the (cleaned) processed macro to the end of the already processed buffer */
          if (selected_macro != NULL
              && (cleaned_macro = clean_macro_chars(selected_macro, macro_options)) != NULL) {
            *output_buffer = resize_string(*output_buffer,
					   strlen(*output_buffer) + strlen(cleaned_macro) + 1);
            strcat(*output_buffer, cleaned_macro);

            log_debug_info(DEBUGL_MACROS, 2,
                           "  Cleaned macro.  Running output (%zd): '%s'\n",
                           strlen(*output_buffer), *output_buffer);
          }
        }
        /* others are not cleaned */
        else {
          /* add the processed macro to the end of the already processed buffer */
          if (selected_macro != NULL) {
            *output_buffer = resize_string(*output_buffer,
					   strlen(*output_buffer) + strlen(selected_macro) + 1);
            strcat(*output_buffer, selected_macro);

            log_debug_info(DEBUGL_MACROS, 2,
                           "  Uncleaned macro.  Running output (%zd): '%s'\n",
                           strlen(*output_buffer), *output_buffer);
          }
        }

        /* free memory if necessary (if we URL encoded the macro or we were told to do so by grab_macro_value()) */
        if (free_macro == TRUE) {
          delete[] selected_macro;
          selected_macro = NULL;
        }
        log_debug_info(DEBUGL_MACROS, 2,
                       "  Just finished macro.  Running output (%zd): '%s'\n",
                       strlen(*output_buffer), *output_buffer);
      }

      in_macro = FALSE;
    }
  }

  /* free copy of input buffer */
  delete[] save_buffer;

  log_debug_info(DEBUGL_MACROS, 1, "  Done.  Final output: '%s'\n", *output_buffer);
  log_debug_info(DEBUGL_MACROS, 1, "**** END MACRO PROCESSING *************\n");
  return (OK);
}

int process_macros(char* input_buffer, char** output_buffer, int options) {
  return (process_macros_r(&global_macros, input_buffer, output_buffer, options));
}

/******************************************************************/
/********************** MACRO GRAB FUNCTIONS **********************/
/******************************************************************/

/* grab macros that are specific to a particular host */
int grab_host_macros(nagios_macros* mac, host* hst) {
  /* clear host-related macros */
  clear_host_macros(mac);
  clear_hostgroup_macros(mac);

  /* save pointer to host*/
  mac->host_ptr = hst;
  mac->hostgroup_ptr = NULL;

  if (hst == NULL)
    return (ERROR);

  /* save pointer to host's first/primary hostgroup */
  if (hst->hostgroups_ptr)
    mac->hostgroup_ptr = (hostgroup*)hst->hostgroups_ptr->object_ptr;
  return (OK);
}

/* grab hostgroup macros */
int grab_hostgroup_macros(nagios_macros* mac, hostgroup* hg) {
  /* clear hostgroup macros */
  clear_hostgroup_macros(mac);

  /* save the hostgroup pointer for later */
  mac->hostgroup_ptr = hg;

  if (hg == NULL)
    return (ERROR);
  return (OK);
}

/* grab macros that are specific to a particular service */
int grab_service_macros(nagios_macros* mac, service* svc) {
  /* clear service-related macros */
  clear_service_macros(mac);
  clear_servicegroup_macros(mac);

  /* save pointer for later */
  mac->service_ptr = svc;
  mac->servicegroup_ptr = NULL;

  if (svc == NULL)
    return (ERROR);

  /* save first/primary servicegroup pointer for later */
  if (svc->servicegroups_ptr)
    mac->servicegroup_ptr = (servicegroup*)svc->servicegroups_ptr->object_ptr;
  return (OK);
}

/* grab macros that are specific to a particular servicegroup */
int grab_servicegroup_macros(nagios_macros* mac, servicegroup* sg) {
  /* clear servicegroup macros */
  clear_servicegroup_macros(mac);

  /* save the pointer for later */
  mac->servicegroup_ptr = sg;

  if (sg == NULL)
    return (ERROR);
  return (OK);
}

/* grab macros that are specific to a particular contact */
int grab_contact_macros(nagios_macros* mac, contact* cntct) {
  /* clear contact-related macros */
  clear_contact_macros(mac);
  clear_contactgroup_macros(mac);

  /* save pointer to contact for later */
  mac->contact_ptr = cntct;
  mac->contactgroup_ptr = NULL;

  if (cntct == NULL)
    return (ERROR);

  /* save pointer to first/primary contactgroup for later */
  if (cntct->contactgroups_ptr)
    mac->contactgroup_ptr = (contactgroup*)cntct->contactgroups_ptr->object_ptr;
  return (OK);
}

/******************************************************************/
/******************* MACRO GENERATION FUNCTIONS *******************/
/******************************************************************/

/* this is the big one */
int grab_macro_value(nagios_macros* mac,
		     char* macro_buffer,
                     char** output,
		     int* clean_options,
                     int* free_macro) {
  char* buf = NULL;
  char* ptr = NULL;
  char* macro_name = NULL;
  char* arg[2] = { NULL, NULL };
  contact* temp_contact = NULL;
  contactgroup* temp_contactgroup = NULL;
  contactsmember* temp_contactsmember = NULL;
  char* temp_buffer = NULL;
  int delimiter_len = 0;
  unsigned int x;
  int result = OK;

  if (output == NULL)
    return (ERROR);

  /* clear the old macro value */
  delete[] *output;
  *output = NULL;

  if (macro_buffer == NULL || clean_options == NULL
      || free_macro == NULL)
    return (ERROR);

  /* work with a copy of the original buffer */
  buf = my_strdup(macro_buffer);

  /* BY DEFAULT, TELL CALLER TO FREE MACRO BUFFER WHEN DONE */
  *free_macro = TRUE;

  /* macro name is at start of buffer */
  macro_name = buf;

  /* see if there's an argument - if so, this is most likely an on-demand macro */
  if ((ptr = strchr(buf, ':'))) {
    ptr[0] = '\x0';
    ptr++;

    /* save the first argument - host name, hostgroup name, etc. */
    arg[0] = ptr;

    /* try and find a second argument */
    if ((ptr = strchr(ptr, ':'))) {
      ptr[0] = '\x0';
      ptr++;

      /* save second argument - service description or delimiter */
      arg[1] = ptr;
    }
  }

  /***** X MACROS *****/
  /* see if this is an x macro */
  for (x = 0; x < MACRO_X_COUNT; x++) {
    if (macro_x_names[x] == NULL)
      continue;

    if (!strcmp(macro_name, macro_x_names[x])) {
      log_debug_info(DEBUGL_MACROS, 2, "  macros[%d] (%s) match.\n", x, macro_x_names[x]);

      /* get the macro value */
      result = grab_macrox_value(mac, x, arg[0], arg[1], output, free_macro);

      /* post-processing */
      /* host/service output/perfdata and author/comment macros should get cleaned */
      if ((x >= 16 && x <= 19) || (x >= 49 && x <= 52)
          || (x >= 99 && x <= 100) || (x >= 124 && x <= 127)) {
        *clean_options |= (STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS);
        log_debug_info(DEBUGL_MACROS, 2, "  New clean options: %d\n", *clean_options);
      }
      /* url macros should get cleaned */
      if ((x >= 125 && x <= 126) || (x >= 128 && x <= 129)
          || (x >= 77 && x <= 78) || (x >= 74 && x <= 75)) {
        *clean_options |= URL_ENCODE_MACRO_CHARS;
        log_debug_info(DEBUGL_MACROS, 2, "  New clean options: %d\n", *clean_options);
      }

      break;
    }
  }

  /* we already found the macro... */
  if (x < MACRO_X_COUNT)
    x = x;
  /***** ARGV MACROS *****/
  else if (strstr(macro_name, "ARG") == macro_name) {
    /* which arg do we want? */
    x = atoi(macro_name + 3);

    if (x <= 0 || x > MAX_COMMAND_ARGUMENTS) {
      delete[] buf;
      return (ERROR);
    }

    /* use a pre-computed macro value */
    *output = mac->argv[x - 1];
    *free_macro = FALSE;
  }
  /***** USER MACROS *****/
  else if (strstr(macro_name, "USER") == macro_name) {
    /* which macro do we want? */
    x = atoi(macro_name + 4);

    if (x <= 0 || x > MAX_USER_MACROS) {
      delete[] buf;
      return (ERROR);
    }

    /* use a pre-computed macro value */
    *output = macro_user[x - 1];
    *free_macro = FALSE;
  }

  /***** CONTACT ADDRESS MACROS *****/
  /* NOTE: the code below should be broken out into a separate function */
  else if (strstr(macro_name, "CONTACTADDRESS") == macro_name) {
    /* which address do we want? */
    x = atoi(macro_name + 14) - 1;

    /* regular macro */
    if (arg[0] == NULL) {
      /* use the saved pointer */
      if ((temp_contact = mac->contact_ptr) == NULL) {
        delete[] buf;
        return (ERROR);
      }

      /* get the macro value */
      result = grab_contact_address_macro(x, temp_contact, output);
    }
    /* on-demand macro */
    else {
      /* on-demand contact macro with a contactgroup and a delimiter */
      if (arg[1] != NULL) {
        if ((temp_contactgroup = find_contactgroup(arg[0])) == NULL)
          return (ERROR);

        delimiter_len = strlen(arg[1]);

        /* concatenate macro values for all contactgroup members */
        for (temp_contactsmember = temp_contactgroup->members;
             temp_contactsmember != NULL;
             temp_contactsmember = temp_contactsmember->next) {

          if ((temp_contact = temp_contactsmember->contact_ptr) == NULL)
            continue;
          if ((temp_contact = find_contact(temp_contactsmember->contact_name)) == NULL)
            continue;

          /* get the macro value for this contact */
          grab_contact_address_macro(x, temp_contact, &temp_buffer);

          if (temp_buffer == NULL)
            continue;

          /* add macro value to already running macro */
          if (*output == NULL)
            *output = my_strdup(temp_buffer);
          else {
            *output = resize_string(*output,
				    strlen(*output) + strlen(temp_buffer) + delimiter_len + 1);
            strcat(*output, arg[1]);
            strcat(*output, temp_buffer);
          }
          delete[] temp_buffer;
          temp_buffer = NULL;
        }
      }
      /* else on-demand contact macro */
      else {
        /* find the contact */
        if ((temp_contact = find_contact(arg[0])) == NULL) {
          delete[] buf;
          return (ERROR);
        }

        /* get the macro value */
        result = grab_contact_address_macro(x, temp_contact, output);
      }
    }
  }
  /***** CUSTOM VARIABLE MACROS *****/
  else if (macro_name[0] == '_') {
    /* get the macro value */
    result = grab_custom_macro_value(mac, macro_name, arg[0], arg[1], output);
  }
  /* no macro matched... */
  else {
    log_debug_info(DEBUGL_MACROS, 0,
                   " WARNING: Could not find a macro matching '%s'!\n",
                   macro_name);
    result = ERROR;
  }

  /* free memory */
  delete[] buf;
  return (result);
}

int grab_macrox_value(nagios_macros* mac,
		      int macro_type,
		      char* arg1,
                      char* arg2,
		      char** output,
		      int* free_macro) {
  host* temp_host = NULL;
  hostgroup* temp_hostgroup = NULL;
  hostsmember* temp_hostsmember = NULL;
  service* temp_service = NULL;
  servicegroup* temp_servicegroup = NULL;
  servicesmember* temp_servicesmember = NULL;
  contact* temp_contact = NULL;
  contactgroup* temp_contactgroup = NULL;
  contactsmember* temp_contactsmember = NULL;
  char* temp_buffer = NULL;
  int result = OK;
  int delimiter_len = 0;
  int free_sub_macro = FALSE;
  unsigned int x;
  int authorized = TRUE;
  int problem = TRUE;
  int hosts_up = 0;
  int hosts_down = 0;
  int hosts_unreachable = 0;
  int hosts_down_unhandled = 0;
  int hosts_unreachable_unhandled = 0;
  int host_problems = 0;
  int host_problems_unhandled = 0;
  int services_ok = 0;
  int services_warning = 0;
  int services_unknown = 0;
  int services_critical = 0;
  int services_warning_unhandled = 0;
  int services_unknown_unhandled = 0;
  int services_critical_unhandled = 0;
  int service_problems = 0;
  int service_problems_unhandled = 0;

  if (output == NULL || free_macro == NULL)
    return (ERROR);

  /* BY DEFAULT, TELL CALLER TO FREE MACRO BUFFER WHEN DONE */
  *free_macro = TRUE;

  /* handle the macro */
  switch (macro_type) {
    /***************/
    /* HOST MACROS */
    /***************/
  case MACRO_HOSTNAME:
  case MACRO_HOSTALIAS:
  case MACRO_HOSTADDRESS:
  case MACRO_LASTHOSTCHECK:
  case MACRO_LASTHOSTSTATECHANGE:
  case MACRO_HOSTOUTPUT:
  case MACRO_HOSTPERFDATA:
  case MACRO_HOSTSTATE:
  case MACRO_HOSTSTATEID:
  case MACRO_HOSTATTEMPT:
  case MACRO_HOSTEXECUTIONTIME:
  case MACRO_HOSTLATENCY:
  case MACRO_HOSTDURATION:
  case MACRO_HOSTDURATIONSEC:
  case MACRO_HOSTDOWNTIME:
  case MACRO_HOSTSTATETYPE:
  case MACRO_HOSTPERCENTCHANGE:
  case MACRO_HOSTACKAUTHOR:
  case MACRO_HOSTACKCOMMENT:
  case MACRO_LASTHOSTUP:
  case MACRO_LASTHOSTDOWN:
  case MACRO_LASTHOSTUNREACHABLE:
  case MACRO_HOSTCHECKCOMMAND:
  case MACRO_HOSTDISPLAYNAME:
  case MACRO_HOSTACTIONURL:
  case MACRO_HOSTNOTESURL:
  case MACRO_HOSTNOTES:
  case MACRO_HOSTCHECKTYPE:
  case MACRO_LONGHOSTOUTPUT:
  case MACRO_HOSTNOTIFICATIONNUMBER:
  case MACRO_HOSTNOTIFICATIONID:
  case MACRO_HOSTEVENTID:
  case MACRO_LASTHOSTEVENTID:
  case MACRO_HOSTGROUPNAMES:
  case MACRO_HOSTACKAUTHORNAME:
  case MACRO_HOSTACKAUTHORALIAS:
  case MACRO_MAXHOSTATTEMPTS:
  case MACRO_TOTALHOSTSERVICES:
  case MACRO_TOTALHOSTSERVICESOK:
  case MACRO_TOTALHOSTSERVICESWARNING:
  case MACRO_TOTALHOSTSERVICESUNKNOWN:
  case MACRO_TOTALHOSTSERVICESCRITICAL:
  case MACRO_HOSTPROBLEMID:
  case MACRO_LASTHOSTPROBLEMID:
  case MACRO_LASTHOSTSTATE:
  case MACRO_LASTHOSTSTATEID:
    /* a standard host macro */
    if (arg2 == NULL) {
      /* find the host for on-demand macros */
      if (arg1) {
        if ((temp_host = find_host(arg1)) == NULL)
          return (ERROR);
      }
      /* else use saved host pointer */
      else if ((temp_host = mac->host_ptr) == NULL)
        return (ERROR);
      /* get the host macro value */
      result = grab_standard_host_macro(mac, macro_type, temp_host, output, free_macro);
    }
    /* a host macro with a hostgroup name and delimiter */
    else {
      if ((temp_hostgroup = find_hostgroup(arg1)) == NULL)
        return (ERROR);

      delimiter_len = strlen(arg2);

      /* concatenate macro values for all hostgroup members */
      for (temp_hostsmember = temp_hostgroup->members;
           temp_hostsmember != NULL;
           temp_hostsmember = temp_hostsmember->next) {

        if ((temp_host = temp_hostsmember->host_ptr) == NULL)
          continue;

        /* get the macro value for this host */
        grab_standard_host_macro(mac, macro_type, temp_host, &temp_buffer, &free_sub_macro);

        if (temp_buffer == NULL)
          continue;

        /* add macro value to already running macro */
        if (*output == NULL)
          *output = my_strdup(temp_buffer);
        else {
          *output = resize_string(*output,
				  strlen(*output) + strlen(temp_buffer) + delimiter_len + 1);
          strcat(*output, arg2);
          strcat(*output, temp_buffer);
        }
        if (free_sub_macro == TRUE) {
          delete[] temp_buffer;
          temp_buffer = NULL;
        }
      }
    }
    break;

    /********************/
    /* HOSTGROUP MACROS */
    /********************/
  case MACRO_HOSTGROUPNAME:
  case MACRO_HOSTGROUPALIAS:
  case MACRO_HOSTGROUPNOTES:
  case MACRO_HOSTGROUPNOTESURL:
  case MACRO_HOSTGROUPACTIONURL:
  case MACRO_HOSTGROUPMEMBERS:
    /* a standard hostgroup macro */
    /* use the saved hostgroup pointer */
    if (arg1 == NULL) {
      if ((temp_hostgroup = mac->hostgroup_ptr) == NULL)
        return (ERROR);
    }
    /* else find the hostgroup for on-demand macros */
    else {
      if ((temp_hostgroup = find_hostgroup(arg1)) == NULL)
        return (ERROR);
    }

    /* get the hostgroup macro value */
    result = grab_standard_hostgroup_macro(mac, macro_type, temp_hostgroup, output);
    break;

    /******************/
    /* SERVICE MACROS */
    /******************/
  case MACRO_SERVICEDESC:
  case MACRO_SERVICESTATE:
  case MACRO_SERVICESTATEID:
  case MACRO_SERVICEATTEMPT:
  case MACRO_LASTSERVICECHECK:
  case MACRO_LASTSERVICESTATECHANGE:
  case MACRO_SERVICEOUTPUT:
  case MACRO_SERVICEPERFDATA:
  case MACRO_SERVICEEXECUTIONTIME:
  case MACRO_SERVICELATENCY:
  case MACRO_SERVICEDURATION:
  case MACRO_SERVICEDURATIONSEC:
  case MACRO_SERVICEDOWNTIME:
  case MACRO_SERVICESTATETYPE:
  case MACRO_SERVICEPERCENTCHANGE:
  case MACRO_SERVICEACKAUTHOR:
  case MACRO_SERVICEACKCOMMENT:
  case MACRO_LASTSERVICEOK:
  case MACRO_LASTSERVICEWARNING:
  case MACRO_LASTSERVICEUNKNOWN:
  case MACRO_LASTSERVICECRITICAL:
  case MACRO_SERVICECHECKCOMMAND:
  case MACRO_SERVICEDISPLAYNAME:
  case MACRO_SERVICEACTIONURL:
  case MACRO_SERVICENOTESURL:
  case MACRO_SERVICENOTES:
  case MACRO_SERVICECHECKTYPE:
  case MACRO_LONGSERVICEOUTPUT:
  case MACRO_SERVICENOTIFICATIONNUMBER:
  case MACRO_SERVICENOTIFICATIONID:
  case MACRO_SERVICEEVENTID:
  case MACRO_LASTSERVICEEVENTID:
  case MACRO_SERVICEGROUPNAMES:
  case MACRO_SERVICEACKAUTHORNAME:
  case MACRO_SERVICEACKAUTHORALIAS:
  case MACRO_MAXSERVICEATTEMPTS:
  case MACRO_SERVICEISVOLATILE:
  case MACRO_SERVICEPROBLEMID:
  case MACRO_LASTSERVICEPROBLEMID:
  case MACRO_LASTSERVICESTATE:
  case MACRO_LASTSERVICESTATEID:
    /* use saved service pointer */
    if (arg1 == NULL && arg2 == NULL) {
      if ((temp_service = mac->service_ptr) == NULL)
        return (ERROR);

      result = grab_standard_service_macro(mac, macro_type, temp_service, output, free_macro);
    }
    /* else and ondemand macro... */
    else {

      /* if first arg is blank, it means use the current host name */
      if (arg1 == NULL || arg1[0] == '\x0') {
        if (mac->host_ptr == NULL)
          return (ERROR);

        if ((temp_service = find_service(mac->host_ptr->name, arg2))) {
          /* get the service macro value */
          result = grab_standard_service_macro(mac, macro_type, temp_service, output, free_macro);
        }
      }
      /* on-demand macro with both host and service name */
      else if ((temp_service = find_service(arg1, arg2))) {
        /* get the service macro value */
        result = grab_standard_service_macro(mac, macro_type, temp_service, output, free_macro);
      }
      /* else we have a service macro with a servicegroup name and a delimiter... */
      else if (arg1 && arg2) {
        if ((temp_servicegroup = find_servicegroup(arg1)) == NULL)
          return (ERROR);

        delimiter_len = strlen(arg2);

        /* concatenate macro values for all servicegroup members */
        for (temp_servicesmember = temp_servicegroup->members;
             temp_servicesmember != NULL;
             temp_servicesmember = temp_servicesmember->next) {

          if ((temp_service = temp_servicesmember->service_ptr) == NULL)
            continue;

          /* get the macro value for this service */
          grab_standard_service_macro(mac,
				      macro_type,
				      temp_service,
                                      &temp_buffer,
				      &free_sub_macro);

          if (temp_buffer == NULL)
            continue;

          /* add macro value to already running macro */
          if (*output == NULL)
            *output = my_strdup(temp_buffer);
          else {
            *output = resize_string(*output,
				    strlen(*output) + strlen(temp_buffer) + delimiter_len + 1);
            strcat(*output, arg2);
            strcat(*output, temp_buffer);
          }
          if (free_sub_macro == TRUE) {
            delete[] temp_buffer;
            temp_buffer = NULL;
          }
        }
      }
      else
        return (ERROR);
    }
    break;

    /***********************/
    /* SERVICEGROUP MACROS */
    /***********************/
  case MACRO_SERVICEGROUPNAME:
  case MACRO_SERVICEGROUPALIAS:
  case MACRO_SERVICEGROUPNOTES:
  case MACRO_SERVICEGROUPNOTESURL:
  case MACRO_SERVICEGROUPACTIONURL:
  case MACRO_SERVICEGROUPMEMBERS:
    /* a standard servicegroup macro */
    /* use the saved servicegroup pointer */
    if (arg1 == NULL) {
      if ((temp_servicegroup = mac->servicegroup_ptr) == NULL)
        return (ERROR);
    }
    /* else find the servicegroup for on-demand macros */
    else {
      if ((temp_servicegroup = find_servicegroup(arg1)) == NULL)
        return (ERROR);
    }

    /* get the servicegroup macro value */
    result = grab_standard_servicegroup_macro(mac, macro_type, temp_servicegroup, output);
    break;

    /******************/
    /* CONTACT MACROS */
    /******************/
  case MACRO_CONTACTNAME:
  case MACRO_CONTACTALIAS:
  case MACRO_CONTACTEMAIL:
  case MACRO_CONTACTPAGER:
  case MACRO_CONTACTGROUPNAMES:
    /* a standard contact macro */
    if (arg2 == NULL) {
      /* find the contact for on-demand macros */
      if (arg1) {
        if ((temp_contact = find_contact(arg1)) == NULL)
          return (ERROR);
      }
      /* else use saved contact pointer */
      else if ((temp_contact = mac->contact_ptr) == NULL)
        return (ERROR);

      /* get the contact macro value */
      result = grab_standard_contact_macro(mac, macro_type, temp_contact, output);
    }
    /* a contact macro with a contactgroup name and delimiter */
    else {
      if ((temp_contactgroup = find_contactgroup(arg1)) == NULL)
        return (ERROR);

      delimiter_len = strlen(arg2);

      /* concatenate macro values for all contactgroup members */
      for (temp_contactsmember = temp_contactgroup->members;
           temp_contactsmember != NULL;
           temp_contactsmember = temp_contactsmember->next) {

        if ((temp_contact = temp_contactsmember->contact_ptr) == NULL)
          continue;

        /* get the macro value for this contact */
        grab_standard_contact_macro(mac,
				    macro_type,
				    temp_contact,
                                    &temp_buffer);

        if (temp_buffer == NULL)
          continue;

        /* add macro value to already running macro */
        if (*output == NULL)
          *output = my_strdup(temp_buffer);
        else {
          *output = resize_string(*output, strlen(*output) + strlen(temp_buffer) + delimiter_len + 1);
          strcat(*output, arg2);
          strcat(*output, temp_buffer);
        }
        delete[] temp_buffer;
        temp_buffer = NULL;
      }
    }
    break;

    /***********************/
    /* CONTACTGROUP MACROS */
    /***********************/
  case MACRO_CONTACTGROUPNAME:
  case MACRO_CONTACTGROUPALIAS:
  case MACRO_CONTACTGROUPMEMBERS:
    /* a standard contactgroup macro */
    /* use the saved contactgroup pointer */
    if (arg1 == NULL) {
      if ((temp_contactgroup = mac->contactgroup_ptr) == NULL)
        return (ERROR);
    }
    /* else find the contactgroup for on-demand macros */
    else {
      if ((temp_contactgroup = find_contactgroup(arg1)) == NULL)
        return (ERROR);
    }
    /* get the contactgroup macro value */
    result = grab_standard_contactgroup_macro(macro_type, temp_contactgroup, output);
    break;

    /***********************/
    /* NOTIFICATION MACROS */
    /***********************/
  case MACRO_NOTIFICATIONTYPE:
  case MACRO_NOTIFICATIONNUMBER:
  case MACRO_NOTIFICATIONRECIPIENTS:
  case MACRO_NOTIFICATIONISESCALATED:
  case MACRO_NOTIFICATIONAUTHOR:
  case MACRO_NOTIFICATIONAUTHORNAME:
  case MACRO_NOTIFICATIONAUTHORALIAS:
  case MACRO_NOTIFICATIONCOMMENT:
    /* notification macros have already been pre-computed */
    *output = mac->x[macro_type];
    *free_macro = FALSE;
    break;

    /********************/
    /* DATE/TIME MACROS */
    /********************/
  case MACRO_LONGDATETIME:
  case MACRO_SHORTDATETIME:
  case MACRO_DATE:
  case MACRO_TIME:
  case MACRO_TIMET:
  case MACRO_ISVALIDTIME:
  case MACRO_NEXTVALIDTIME:
    /* calculate macros */
    result = grab_datetime_macro(mac, macro_type, arg1, arg2, output);
    break;

    /*****************/
    /* STATIC MACROS */
    /*****************/
  case MACRO_ADMINEMAIL:
  case MACRO_ADMINPAGER:
  case MACRO_MAINCONFIGFILE:
  case MACRO_STATUSDATAFILE:
  case MACRO_RETENTIONDATAFILE:
  case MACRO_OBJECTCACHEFILE:
  case MACRO_TEMPFILE:
  case MACRO_LOGFILE:
  case MACRO_RESOURCEFILE:
  case MACRO_COMMANDFILE:
  case MACRO_HOSTPERFDATAFILE:
  case MACRO_SERVICEPERFDATAFILE:
  case MACRO_PROCESSSTARTTIME:
  case MACRO_TEMPPATH:
  case MACRO_EVENTSTARTTIME:
    /* no need to do any more work - these are already precomputed for us */
    *output = global_macros.x[macro_type];
    *free_macro = FALSE;
    break;

    /******************/
    /* SUMMARY MACROS */
    /******************/
  case MACRO_TOTALHOSTSUP:
  case MACRO_TOTALHOSTSDOWN:
  case MACRO_TOTALHOSTSUNREACHABLE:
  case MACRO_TOTALHOSTSDOWNUNHANDLED:
  case MACRO_TOTALHOSTSUNREACHABLEUNHANDLED:
  case MACRO_TOTALHOSTPROBLEMS:
  case MACRO_TOTALHOSTPROBLEMSUNHANDLED:
  case MACRO_TOTALSERVICESOK:
  case MACRO_TOTALSERVICESWARNING:
  case MACRO_TOTALSERVICESCRITICAL:
  case MACRO_TOTALSERVICESUNKNOWN:
  case MACRO_TOTALSERVICESWARNINGUNHANDLED:
  case MACRO_TOTALSERVICESCRITICALUNHANDLED:
  case MACRO_TOTALSERVICESUNKNOWNUNHANDLED:
  case MACRO_TOTALSERVICEPROBLEMS:
  case MACRO_TOTALSERVICEPROBLEMSUNHANDLED:
    /* generate summary macros if needed */
    if (mac->x[MACRO_TOTALHOSTSUP] == NULL) {
      /* get host totals */
      for (temp_host = host_list;
	   temp_host != NULL;
           temp_host = temp_host->next) {

        /* filter totals based on contact if necessary */
        if (mac->contact_ptr != NULL)
          authorized = is_contact_for_host(temp_host, mac->contact_ptr);

        if (authorized == TRUE) {
          problem = TRUE;

          if (temp_host->current_state == HOST_UP
              && temp_host->has_been_checked == TRUE)
            hosts_up++;
          else if (temp_host->current_state == HOST_DOWN) {
            if (temp_host->scheduled_downtime_depth > 0)
              problem = FALSE;
            if (temp_host->problem_has_been_acknowledged == TRUE)
              problem = FALSE;
            if (temp_host->checks_enabled == FALSE)
              problem = FALSE;
            if (problem == TRUE)
              hosts_down_unhandled++;
            hosts_down++;
          }
          else if (temp_host->current_state == HOST_UNREACHABLE) {
            if (temp_host->scheduled_downtime_depth > 0)
              problem = FALSE;
            if (temp_host->problem_has_been_acknowledged == TRUE)
              problem = FALSE;
            if (temp_host->checks_enabled == FALSE)
              problem = FALSE;
            if (problem == TRUE)
              hosts_down_unhandled++;
            hosts_unreachable++;
          }
        }
      }

      host_problems = hosts_down + hosts_unreachable;
      host_problems_unhandled = hosts_down_unhandled + hosts_unreachable_unhandled;

      /* get service totals */
      for (temp_service = service_list;
	   temp_service != NULL;
           temp_service = temp_service->next) {

        /* filter totals based on contact if necessary */
        if (mac->contact_ptr != NULL)
          authorized = is_contact_for_service(temp_service, mac->contact_ptr);

        if (authorized == TRUE) {
          problem = TRUE;

          if (temp_service->current_state == STATE_OK
              && temp_service->has_been_checked == TRUE)
            services_ok++;
          else if (temp_service->current_state == STATE_WARNING) {
            temp_host = find_host(temp_service->host_name);
            if (temp_host != NULL
                && (temp_host->current_state == HOST_DOWN
                    || temp_host->current_state == HOST_UNREACHABLE))
              problem = FALSE;
            if (temp_service->scheduled_downtime_depth > 0)
              problem = FALSE;
            if (temp_service->problem_has_been_acknowledged == TRUE)
              problem = FALSE;
            if (temp_service->checks_enabled == FALSE)
              problem = FALSE;
            if (problem == TRUE)
              services_warning_unhandled++;
            services_warning++;
          }
          else if (temp_service->current_state == STATE_UNKNOWN) {
            temp_host = find_host(temp_service->host_name);
            if (temp_host != NULL
                && (temp_host->current_state == HOST_DOWN
                    || temp_host->current_state == HOST_UNREACHABLE))
              problem = FALSE;
            if (temp_service->scheduled_downtime_depth > 0)
              problem = FALSE;
            if (temp_service->problem_has_been_acknowledged == TRUE)
              problem = FALSE;
            if (temp_service->checks_enabled == FALSE)
              problem = FALSE;
            if (problem == TRUE)
              services_unknown_unhandled++;
            services_unknown++;
          }
          else if (temp_service->current_state == STATE_CRITICAL) {
            temp_host = find_host(temp_service->host_name);
            if (temp_host != NULL
                && (temp_host->current_state == HOST_DOWN
                    || temp_host->current_state == HOST_UNREACHABLE))
              problem = FALSE;
            if (temp_service->scheduled_downtime_depth > 0)
              problem = FALSE;
            if (temp_service->problem_has_been_acknowledged == TRUE)
              problem = FALSE;
            if (temp_service->checks_enabled == FALSE)
              problem = FALSE;
            if (problem == TRUE)
              services_critical_unhandled++;
            services_critical++;
          }
        }
      }

      service_problems = services_warning + services_critical + services_unknown;
      service_problems_unhandled = services_warning_unhandled + services_critical_unhandled + services_unknown_unhandled;

      /* these macros are time-intensive to compute, and will likely be used together, so save them all for future use */
      for (x = MACRO_TOTALHOSTSUP; x <= MACRO_TOTALSERVICEPROBLEMSUNHANDLED; x++) {
        delete[] mac->x[x];
        mac->x[x] = NULL;
      }
      mac->x[MACRO_TOTALHOSTSUP] = obj2pchar(hosts_up);
      mac->x[MACRO_TOTALHOSTSDOWN] = obj2pchar(hosts_down);
      mac->x[MACRO_TOTALHOSTSUNREACHABLE] = obj2pchar(hosts_unreachable);
      mac->x[MACRO_TOTALHOSTSDOWNUNHANDLED] = obj2pchar(hosts_down_unhandled);
      mac->x[MACRO_TOTALHOSTSUNREACHABLEUNHANDLED] = obj2pchar(hosts_unreachable_unhandled);
      mac->x[MACRO_TOTALHOSTPROBLEMS] = obj2pchar(host_problems);
      mac->x[MACRO_TOTALHOSTPROBLEMSUNHANDLED] = obj2pchar(host_problems_unhandled);
      mac->x[MACRO_TOTALSERVICESOK] = obj2pchar(services_ok);
      mac->x[MACRO_TOTALSERVICESWARNING] = obj2pchar(services_warning);
      mac->x[MACRO_TOTALSERVICESCRITICAL] = obj2pchar(services_critical);
      mac->x[MACRO_TOTALSERVICESUNKNOWN] = obj2pchar(services_unknown);
      mac->x[MACRO_TOTALSERVICESWARNINGUNHANDLED] = obj2pchar(services_warning_unhandled);
      mac->x[MACRO_TOTALSERVICESCRITICALUNHANDLED] = obj2pchar(services_critical_unhandled);
      mac->x[MACRO_TOTALSERVICESUNKNOWNUNHANDLED] = obj2pchar(services_unknown_unhandled);
      mac->x[MACRO_TOTALSERVICEPROBLEMS] = obj2pchar(service_problems);
      mac->x[MACRO_TOTALSERVICEPROBLEMSUNHANDLED] = obj2pchar(service_problems_unhandled);
    }

    /* return only the macro the user requested */
    *output = mac->x[macro_type];

    /* tell caller to NOT free memory when done */
    *free_macro = FALSE;
    break;

  default:
    log_debug_info(DEBUGL_MACROS, 0,
                   "UNHANDLED MACRO #%d! THIS IS A BUG!\n", macro_type);
    return (ERROR);
  }
  return (result);
}

/* calculates the value of a custom macro */
int grab_custom_macro_value(nagios_macros* mac, char* macro_name,
                            char* arg1, char* arg2, char** output) {
  host* temp_host = NULL;
  hostgroup* temp_hostgroup = NULL;
  hostsmember* temp_hostsmember = NULL;
  service* temp_service = NULL;
  servicegroup* temp_servicegroup = NULL;
  servicesmember* temp_servicesmember = NULL;
  contact* temp_contact = NULL;
  contactgroup* temp_contactgroup = NULL;
  contactsmember* temp_contactsmember = NULL;
  int delimiter_len = 0;
  char* temp_buffer = NULL;
  int result = OK;

  if (macro_name == NULL || output == NULL)
    return (ERROR);

  /***** CUSTOM HOST MACRO *****/
  if (strstr(macro_name, "_HOST") == macro_name) {
    /* a standard host macro */
    if (arg2 == NULL) {
      /* find the host for on-demand macros */
      if (arg1) {
        if ((temp_host = find_host(arg1)) == NULL)
          return (ERROR);
      }
      /* else use saved host pointer */
      else if ((temp_host = mac->host_ptr) == NULL)
        return (ERROR);

      /* get the host macro value */
      result = grab_custom_object_macro(mac, macro_name + 5, temp_host->custom_variables, output);
    }
    /* a host macro with a hostgroup name and delimiter */
    else {
      if ((temp_hostgroup = find_hostgroup(arg1)) == NULL)
        return (ERROR);

      delimiter_len = strlen(arg2);

      /* concatenate macro values for all hostgroup members */
      for (temp_hostsmember = temp_hostgroup->members;
           temp_hostsmember != NULL;
           temp_hostsmember = temp_hostsmember->next) {

        if ((temp_host = temp_hostsmember->host_ptr) == NULL)
          continue;

        /* get the macro value for this host */
        grab_custom_macro_value(mac, macro_name, temp_host->name, NULL,
                                &temp_buffer);

        if (temp_buffer == NULL)
          continue;

        /* add macro value to already running macro */
        if (*output == NULL)
          *output = my_strdup(temp_buffer);
        else {
          *output = resize_string(*output,
				  strlen(*output) + strlen(temp_buffer) + delimiter_len + 1);
          strcat(*output, arg2);
          strcat(*output, temp_buffer);
        }
        delete[] temp_buffer;
        temp_buffer = NULL;
      }
    }
  }
  /***** CUSTOM SERVICE MACRO *****/
  else if (strstr(macro_name, "_SERVICE") == macro_name) {
    /* use saved service pointer */
    if (arg1 == NULL && arg2 == NULL) {
      if ((temp_service = mac->service_ptr) == NULL)
        return (ERROR);

      /* get the service macro value */
      result = grab_custom_object_macro(mac, macro_name + 8, temp_service->custom_variables, output);
    }
    /* else and ondemand macro... */
    else {
      /* if first arg is blank, it means use the current host name */
      if (mac->host_ptr == NULL)
        return (ERROR);
      if ((temp_service = find_service((mac->host_ptr) ? mac->host_ptr->name : NULL, arg2))) {
        /* get the service macro value */
        result = grab_custom_object_macro(mac, macro_name + 8,
					  temp_service->custom_variables,
					  output);
      }
      /* else we have a service macro with a servicegroup name and a delimiter... */
      else {
        if ((temp_servicegroup = find_servicegroup(arg1)) == NULL)
          return (ERROR);

        delimiter_len = strlen(arg2);

        /* concatenate macro values for all servicegroup members */
        for (temp_servicesmember = temp_servicegroup->members;
             temp_servicesmember != NULL;
             temp_servicesmember = temp_servicesmember->next) {

          if ((temp_service = temp_servicesmember->service_ptr) == NULL)
            continue;

          /* get the macro value for this service */
          grab_custom_macro_value(mac,
				  macro_name,
                                  temp_service->host_name,
                                  temp_service->description,
                                  &temp_buffer);

          if (temp_buffer == NULL)
            continue;

          /* add macro value to already running macro */
          if (*output == NULL)
            *output = my_strdup(temp_buffer);
          else {
            *output = resize_string(*output,
				    strlen(*output) + strlen(temp_buffer) + delimiter_len + 1);
            strcat(*output, arg2);
            strcat(*output, temp_buffer);
          }
          delete[] temp_buffer;
          temp_buffer = NULL;
        }
      }
    }
  }
  /***** CUSTOM CONTACT VARIABLE *****/
  else if (strstr(macro_name, "_CONTACT") == macro_name) {
    /* a standard contact macro */
    if (arg2 == NULL) {
      /* find the contact for on-demand macros */
      if (arg1) {
        if ((temp_contact = find_contact(arg1)) == NULL)
          return (ERROR);
      }
      /* else use saved contact pointer */
      else if ((temp_contact = mac->contact_ptr) == NULL)
        return (ERROR);

      /* get the contact macro value */
      result = grab_custom_object_macro(mac, macro_name + 8,
					temp_contact->custom_variables,
					output);
    }
    /* a contact macro with a contactgroup name and delimiter */
    else {
      if ((temp_contactgroup = find_contactgroup(arg1)) == NULL)
        return (ERROR);

      delimiter_len = strlen(arg2);

      /* concatenate macro values for all contactgroup members */
      for (temp_contactsmember = temp_contactgroup->members;
           temp_contactsmember != NULL;
           temp_contactsmember = temp_contactsmember->next) {

        if ((temp_contact = temp_contactsmember->contact_ptr) == NULL)
          continue;

        /* get the macro value for this contact */
        grab_custom_macro_value(mac,
				macro_name,
				temp_contact->name,
                                NULL,
				&temp_buffer);

        if (temp_buffer == NULL)
          continue;

        /* add macro value to already running macro */
        if (*output == NULL)
          *output = my_strdup(temp_buffer);
        else {
          *output = resize_string(*output,
				  strlen(*output) + strlen(temp_buffer) + delimiter_len + 1);
          strcat(*output, arg2);
          strcat(*output, temp_buffer);
        }
        delete[] temp_buffer;
        temp_buffer = NULL;
      }
    }
  }
  else
    return (ERROR);
  return (result);
}

/* calculates a date/time macro */
int grab_datetime_macro(nagios_macros* mac,
			int macro_type,
			char* arg1,
                        char* arg2,
			char** output) {
  time_t current_time = 0L;
  timeperiod* temp_timeperiod = NULL;
  time_t test_time = 0L;
  time_t next_valid_time = 0L;

  (void)mac;

  if (output == NULL)
    return (ERROR);

  /* get the current time */
  time(&current_time);

  /* parse args, do prep work */
  switch (macro_type) {
  case MACRO_ISVALIDTIME:
  case MACRO_NEXTVALIDTIME:
    /* find the timeperiod */
    if ((temp_timeperiod = find_timeperiod(arg1)) == NULL)
      return (ERROR);
    /* what timestamp should we use? */
    if (arg2)
      test_time = (time_t)strtoul(arg2, NULL, 0);
    else
      test_time = current_time;
    break;

  default:
    break;
  }

  /* calculate the value */
  switch (macro_type) {
  case MACRO_LONGDATETIME:
    if (*output == NULL)
      *output = new char[MAX_DATETIME_LENGTH];
    get_datetime_string(&current_time, *output, MAX_DATETIME_LENGTH, LONG_DATE_TIME);
    break;

  case MACRO_SHORTDATETIME:
    if (*output == NULL)
      *output = new char[MAX_DATETIME_LENGTH];
    get_datetime_string(&current_time, *output, MAX_DATETIME_LENGTH, SHORT_DATE_TIME);
    break;

  case MACRO_DATE:
    if (*output == NULL)
      *output = new char[MAX_DATETIME_LENGTH];
    get_datetime_string(&current_time, *output, MAX_DATETIME_LENGTH, SHORT_DATE);
    break;

  case MACRO_TIME:
    if (*output == NULL)
      *output = new char[MAX_DATETIME_LENGTH];
    get_datetime_string(&current_time, *output, MAX_DATETIME_LENGTH, SHORT_TIME);
    break;

  case MACRO_TIMET:
    *output = obj2pchar<unsigned long>(current_time);
    break;

  case MACRO_ISVALIDTIME:
    *output = obj2pchar((check_time_against_period(test_time, temp_timeperiod) == OK) ? 1 : 0);
    break;

  case MACRO_NEXTVALIDTIME:
    get_next_valid_time(test_time, &next_valid_time, temp_timeperiod);
    if (next_valid_time == test_time
        && check_time_against_period(test_time, temp_timeperiod) == ERROR)
      next_valid_time = (time_t)0L;
    *output = obj2pchar<unsigned long>(next_valid_time);
    break;

  default:
    return (ERROR);
  }
  return (OK);
}

/* calculates a host macro */
int grab_standard_host_macro(nagios_macros* mac,
                             unsigned int macro_type,
			     host* temp_host,
                             char** output,
			     int* free_macro) {
  char* temp_buffer = NULL;
  hostgroup* temp_hostgroup = NULL;
  servicesmember* temp_servicesmember = NULL;
  service* temp_service = NULL;
  objectlist* temp_objectlist = NULL;
  time_t current_time = 0L;
  unsigned long duration = 0L;
  int days = 0;
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  char* buf1 = NULL;
  char* buf2 = NULL;
  int total_host_services = 0;
  int total_host_services_ok = 0;
  int total_host_services_warning = 0;
  int total_host_services_unknown = 0;
  int total_host_services_critical = 0;

  if (temp_host == NULL || output == NULL || free_macro == NULL)
    return (ERROR);

  /* BY DEFAULT TELL CALLER TO FREE MACRO BUFFER WHEN DONE */
  *free_macro = TRUE;

  /* get the macro */
  switch (macro_type) {
  case MACRO_HOSTNAME:
    *output = my_strdup(temp_host->name);
    break;

  case MACRO_HOSTDISPLAYNAME:
    if (temp_host->display_name)
      *output = my_strdup(temp_host->display_name);
    break;

  case MACRO_HOSTALIAS:
    *output = my_strdup(temp_host->alias);
    break;

  case MACRO_HOSTADDRESS:
    *output = my_strdup(temp_host->address);
    break;

  case MACRO_HOSTSTATE:
    if (temp_host->current_state == HOST_DOWN)
      *output = my_strdup("DOWN");
    else if (temp_host->current_state == HOST_UNREACHABLE)
      *output = my_strdup("UNREACHABLE");
    else
      *output = my_strdup("UP");
    break;

  case MACRO_HOSTSTATEID:
    *output = obj2pchar(temp_host->current_state);
    break;

  case MACRO_LASTHOSTSTATE:
    if (temp_host->last_state == HOST_DOWN)
      *output = my_strdup("DOWN");
    else if (temp_host->last_state == HOST_UNREACHABLE)
      *output = my_strdup("UNREACHABLE");
    else
      *output = my_strdup("UP");
    break;

  case MACRO_LASTHOSTSTATEID:
    *output = obj2pchar(temp_host->last_state);
    break;

  case MACRO_HOSTCHECKTYPE:
    *output = my_strdup(temp_host->check_type == HOST_CHECK_PASSIVE ? "PASSIVE" : "ACTIVE");
    break;

  case MACRO_HOSTSTATETYPE:
    *output = my_strdup(temp_host->state_type == HARD_STATE ? "HARD" : "SOFT");
    break;

  case MACRO_HOSTOUTPUT:
    if (temp_host->plugin_output)
      *output = my_strdup(temp_host->plugin_output);
    break;

  case MACRO_LONGHOSTOUTPUT:
    if (temp_host->long_plugin_output)
      *output = my_strdup(temp_host->long_plugin_output);
    break;

  case MACRO_HOSTPERFDATA:
    if (temp_host->perf_data)
      *output = my_strdup(temp_host->perf_data);
    break;

  case MACRO_HOSTCHECKCOMMAND:
    if (temp_host->host_check_command)
      *output = my_strdup(temp_host->host_check_command);
    break;

  case MACRO_HOSTATTEMPT:
    *output = obj2pchar(temp_host->last_state);
    break;

  case MACRO_MAXHOSTATTEMPTS:
    *output = obj2pchar(temp_host->last_state);
    break;

  case MACRO_HOSTDOWNTIME:
    *output = obj2pchar(temp_host->last_state);
    break;

  case MACRO_HOSTPERCENTCHANGE:{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << temp_host->percent_state_change;
    *output = my_strdup(oss.str().c_str());
    break;
  }

  case MACRO_HOSTDURATIONSEC:
  case MACRO_HOSTDURATION:
    time(&current_time);
    duration = (unsigned long)(current_time - temp_host->last_state_change);

    if (macro_type == MACRO_HOSTDURATIONSEC)
      *output = obj2pchar(duration);
    else {
      days = duration / 86400;
      duration -= (days * 86400);
      hours = duration / 3600;
      duration -= (hours * 3600);
      minutes = duration / 60;
      duration -= (minutes * 60);
      seconds = duration;
      std::ostringstream oss;
      oss << days << "d "
	  << hours << "h "
	  << minutes << "m "
	  << seconds << "s";
      *output = my_strdup(oss.str().c_str());
    }
    break;

  case MACRO_HOSTEXECUTIONTIME:{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << temp_host->execution_time;
    *output = my_strdup(oss.str().c_str());
  }
    break;

  case MACRO_HOSTLATENCY:{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << temp_host->latency;
    *output = my_strdup(oss.str().c_str());
  }
    break;

  case MACRO_LASTHOSTCHECK:
    *output = obj2pchar<unsigned long>(temp_host->last_check);
    break;

  case MACRO_LASTHOSTSTATECHANGE:
    *output = obj2pchar<unsigned long>(temp_host->last_state_change);
    break;

  case MACRO_LASTHOSTUP:
    *output = obj2pchar<unsigned long>(temp_host->last_time_up);
    break;

  case MACRO_LASTHOSTDOWN:
    *output = obj2pchar<unsigned long>(temp_host->last_time_down);
    break;

  case MACRO_LASTHOSTUNREACHABLE:
    *output = obj2pchar<unsigned long>(temp_host->last_time_unreachable);
    break;

  case MACRO_HOSTNOTIFICATIONNUMBER:
    *output = obj2pchar(temp_host->last_state);
    break;

  case MACRO_HOSTNOTIFICATIONID:
    *output = obj2pchar(temp_host->current_notification_id);
    break;

  case MACRO_HOSTEVENTID:
    *output = obj2pchar(temp_host->current_event_id);
    break;

  case MACRO_LASTHOSTEVENTID:
    *output = obj2pchar(temp_host->last_event_id);
    break;

  case MACRO_HOSTPROBLEMID:
    *output = obj2pchar(temp_host->current_problem_id);
    break;

  case MACRO_LASTHOSTPROBLEMID:
    *output = obj2pchar(temp_host->last_problem_id);
    break;

  case MACRO_HOSTACTIONURL:
    if (temp_host->action_url)
      *output = my_strdup(temp_host->action_url);
    break;

  case MACRO_HOSTNOTESURL:
    if (temp_host->notes_url)
      *output = my_strdup(temp_host->notes_url);
    break;

  case MACRO_HOSTNOTES:
    if (temp_host->notes)
      *output = my_strdup(temp_host->notes);
    break;

  case MACRO_HOSTGROUPNAMES:
    /* find all hostgroups this host is associated with */
    for (temp_objectlist = temp_host->hostgroups_ptr;
         temp_objectlist != NULL;
         temp_objectlist = temp_objectlist->next) {

      if ((temp_hostgroup = (hostgroup*)temp_objectlist->object_ptr) == NULL)
        continue;

      std::ostringstream oss;
      oss << (buf2 ? buf2 : "") << (buf2 ? "," : "") << temp_hostgroup->group_name;
      buf1 = my_strdup(oss.str().c_str());
      delete[] buf2;
      buf2 = buf1;
    }
    if (buf2) {
      *output = my_strdup(buf2);
      delete[] buf2;
      buf2 = NULL;
    }
    break;

  case MACRO_TOTALHOSTSERVICES:
  case MACRO_TOTALHOSTSERVICESOK:
  case MACRO_TOTALHOSTSERVICESWARNING:
  case MACRO_TOTALHOSTSERVICESUNKNOWN:
  case MACRO_TOTALHOSTSERVICESCRITICAL:
    /* generate host service summary macros (if they haven't already been computed) */
    if (mac->x[MACRO_TOTALHOSTSERVICES] == NULL) {
      for (temp_servicesmember = temp_host->services;
           temp_servicesmember != NULL;
           temp_servicesmember = temp_servicesmember->next) {
        if ((temp_service = temp_servicesmember->service_ptr) == NULL)
          continue;

        total_host_services++;

        switch (temp_service->current_state) {
        case STATE_OK:
          total_host_services_ok++;
          break;

        case STATE_WARNING:
          total_host_services_warning++;
          break;

        case STATE_UNKNOWN:
          total_host_services_unknown++;
          break;

        case STATE_CRITICAL:
          total_host_services_critical++;
          break;

        default:
          break;
        }
      }

      /* these macros are time-intensive to compute, and will likely be used together, so save them all for future use */
      delete[] mac->x[MACRO_TOTALHOSTSERVICES];
      delete[] mac->x[MACRO_TOTALHOSTSERVICESOK];
      delete[] mac->x[MACRO_TOTALHOSTSERVICESWARNING];
      delete[] mac->x[MACRO_TOTALHOSTSERVICESUNKNOWN];
      delete[] mac->x[MACRO_TOTALHOSTSERVICESCRITICAL];
      mac->x[MACRO_TOTALHOSTSERVICES] = obj2pchar(total_host_services);
      mac->x[MACRO_TOTALHOSTSERVICESOK] = obj2pchar(total_host_services_ok);
      mac->x[MACRO_TOTALHOSTSERVICESWARNING] = obj2pchar(total_host_services_warning);
      mac->x[MACRO_TOTALHOSTSERVICESUNKNOWN] = obj2pchar(total_host_services_unknown);
      mac->x[MACRO_TOTALHOSTSERVICESCRITICAL] = obj2pchar(total_host_services_critical);
    }

    /* return only the macro the user requested */
    *output = mac->x[macro_type];

    /* tell caller to NOT free memory when done */
    *free_macro = FALSE;
    break;

    /***************/
    /* MISC MACROS */
    /***************/
  case MACRO_HOSTACKAUTHOR:
  case MACRO_HOSTACKAUTHORNAME:
  case MACRO_HOSTACKAUTHORALIAS:
  case MACRO_HOSTACKCOMMENT:
    /* no need to do any more work - these are already precomputed elsewhere */
    /* NOTE: these macros won't work as on-demand macros */
    *output = mac->x[macro_type];
    *free_macro = FALSE;
    break;

  default:
    log_debug_info(DEBUGL_MACROS, 0,
                   "UNHANDLED HOST MACRO #%d! THIS IS A BUG!\n",
                   macro_type);
    return (ERROR);
  }

  /* post-processing */
  /* notes, notes URL and action URL macros may themselves contain macros, so process them... */
  switch (macro_type) {
  case MACRO_HOSTACTIONURL:
  case MACRO_HOSTNOTESURL:
    process_macros_r(mac, *output, &temp_buffer, URL_ENCODE_MACRO_CHARS);
    delete[] *output;
    *output = temp_buffer;
    break;

  case MACRO_HOSTNOTES:
    process_macros_r(mac, *output, &temp_buffer, 0);
    delete[] *output;
    *output = temp_buffer;
    break;

  default:
    break;
  }

  return (OK);
}

/* computes a hostgroup macro */
int grab_standard_hostgroup_macro(nagios_macros* mac,
				  int macro_type,
                                  hostgroup* temp_hostgroup,
                                  char** output) {
  hostsmember* temp_hostsmember = NULL;
  char* temp_buffer = NULL;
  unsigned int temp_len = 0;
  unsigned int init_len = 0;

  if (temp_hostgroup == NULL || output == NULL)
    return (ERROR);

  /* get the macro value */
  switch (macro_type) {
  case MACRO_HOSTGROUPNAME:
    *output = my_strdup(temp_hostgroup->group_name);
    break;

  case MACRO_HOSTGROUPALIAS:
    if (temp_hostgroup->alias)
      *output = my_strdup(temp_hostgroup->alias);
    break;

  case MACRO_HOSTGROUPMEMBERS:
    /* make the calculations for total string length */
    for (temp_hostsmember = temp_hostgroup->members;
         temp_hostsmember != NULL;
         temp_hostsmember = temp_hostsmember->next) {
      if (temp_hostsmember->host_name == NULL)
        continue;
      if (temp_len == 0)
        temp_len += strlen(temp_hostsmember->host_name) + 1;
      else
        temp_len += strlen(temp_hostsmember->host_name) + 2;
    }
    /* allocate or reallocate the memory buffer */
    if (*output == NULL)
      *output = new char[temp_len];
    else {
      init_len = strlen(*output);
      temp_len += init_len;
      *output = resize_string(*output, temp_len);
    }
    /* now fill in the string with the member names */
    for (temp_hostsmember = temp_hostgroup->members;
         temp_hostsmember != NULL;
         temp_hostsmember = temp_hostsmember->next) {
      if (temp_hostsmember->host_name == NULL)
        continue;
      temp_buffer = *output + init_len;
      if (init_len == 0)      /* If our buffer didn't contain anything, we just need to write "%s,%s" */
        init_len += sprintf(temp_buffer, "%s", temp_hostsmember->host_name);
      else
        init_len += sprintf(temp_buffer, ",%s", temp_hostsmember->host_name);
    }
    break;

  case MACRO_HOSTGROUPACTIONURL:
    if (temp_hostgroup->action_url)
      *output = my_strdup(temp_hostgroup->action_url);
    break;

  case MACRO_HOSTGROUPNOTESURL:
    if (temp_hostgroup->notes_url)
      *output = my_strdup(temp_hostgroup->notes_url);
    break;

  case MACRO_HOSTGROUPNOTES:
    if (temp_hostgroup->notes)
      *output = my_strdup(temp_hostgroup->notes);
    break;

  default:
    log_debug_info(DEBUGL_MACROS, 0,
                   "UNHANDLED HOSTGROUP MACRO #%d! THIS IS A BUG!\n",
                   macro_type);
    return (ERROR);
  }

  /* post-processing */
  /* notes, notes URL and action URL macros may themselves contain macros, so process them... */
  switch (macro_type) {
  case MACRO_HOSTGROUPACTIONURL:
  case MACRO_HOSTGROUPNOTESURL:
    process_macros_r(mac, *output, &temp_buffer, URL_ENCODE_MACRO_CHARS);
    delete[] *output;
    *output = temp_buffer;
    break;

  case MACRO_HOSTGROUPNOTES:
    process_macros_r(mac, *output, &temp_buffer, 0);
    delete[] *output;
    *output = temp_buffer;
    break;

  default:
    break;
  }

  return (OK);
}

/* computes a service macro */
int grab_standard_service_macro(nagios_macros* mac,
                                unsigned int macro_type,
                                service* temp_service,
				char** output,
                                int* free_macro) {
  char* temp_buffer = NULL;
  servicegroup* temp_servicegroup = NULL;
  objectlist* temp_objectlist = NULL;
  time_t current_time = 0L;
  unsigned long duration = 0L;
  int days = 0;
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  char* buf1 = NULL;
  char* buf2 = NULL;

  if (temp_service == NULL || output == NULL)
    return (ERROR);

  /* BY DEFAULT TELL CALLER TO FREE MACRO BUFFER WHEN DONE */
  *free_macro = TRUE;

  /* get the macro value */
  switch (macro_type) {
  case MACRO_SERVICEDESC:
    *output = my_strdup(temp_service->description);
    break;

  case MACRO_SERVICEDISPLAYNAME:
    if (temp_service->display_name)
      *output = my_strdup(temp_service->display_name);
    break;

  case MACRO_SERVICEOUTPUT:
    if (temp_service->plugin_output)
      *output = my_strdup(temp_service->plugin_output);
    break;

  case MACRO_LONGSERVICEOUTPUT:
    if (temp_service->long_plugin_output)
      *output = my_strdup(temp_service->long_plugin_output);
    break;

  case MACRO_SERVICEPERFDATA:
    if (temp_service->perf_data)
      *output = my_strdup(temp_service->perf_data);
    break;

  case MACRO_SERVICECHECKCOMMAND:
    if (temp_service->service_check_command)
      *output = my_strdup(temp_service->service_check_command);
    break;

  case MACRO_SERVICECHECKTYPE:
    *output = my_strdup((temp_service->check_type == SERVICE_CHECK_PASSIVE) ? "PASSIVE" : "ACTIVE");
    break;

  case MACRO_SERVICESTATETYPE:
    *output = my_strdup((temp_service->state_type == HARD_STATE) ? "HARD" : "SOFT");
    break;

  case MACRO_SERVICESTATE:
    if (temp_service->current_state == STATE_OK)
      *output = my_strdup("OK");
    else if (temp_service->current_state == STATE_WARNING)
      *output = my_strdup("WARNING");
    else if (temp_service->current_state == STATE_CRITICAL)
      *output = my_strdup("CRITICAL");
    else
      *output = my_strdup("UNKNOWN");
    break;

  case MACRO_SERVICESTATEID:
    *output = obj2pchar(temp_service->last_state);
    break;

  case MACRO_LASTSERVICESTATE:
    if (temp_service->last_state == STATE_OK)
      *output = my_strdup("OK");
    else if (temp_service->last_state == STATE_WARNING)
      *output = my_strdup("WARNING");
    else if (temp_service->last_state == STATE_CRITICAL)
      *output = my_strdup("CRITICAL");
    else
      *output = my_strdup("UNKNOWN");
    break;

  case MACRO_LASTSERVICESTATEID:
    *output = obj2pchar(temp_service->last_state);
    break;

  case MACRO_SERVICEISVOLATILE:
    *output = obj2pchar(temp_service->last_state);
    break;

  case MACRO_SERVICEATTEMPT:
    *output = obj2pchar(temp_service->last_state);
    break;

  case MACRO_MAXSERVICEATTEMPTS:
    *output = obj2pchar(temp_service->last_state);
    break;

  case MACRO_SERVICEEXECUTIONTIME:{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << temp_service->execution_time;
    *output = my_strdup(oss.str().c_str());
  }
    break;

  case MACRO_SERVICELATENCY:{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << temp_service->latency;
    *output = my_strdup(oss.str().c_str());
  }
    break;

  case MACRO_LASTSERVICECHECK:
    *output = obj2pchar<unsigned long>(temp_service->last_check);
    break;

  case MACRO_LASTSERVICESTATECHANGE:
    *output = obj2pchar<unsigned long>(temp_service->last_state_change);
    break;

  case MACRO_LASTSERVICEOK:
    *output = obj2pchar<unsigned long>(temp_service->last_time_ok);
    break;

  case MACRO_LASTSERVICEWARNING:
    *output = obj2pchar<unsigned long>(temp_service->last_time_warning);
    break;

  case MACRO_LASTSERVICEUNKNOWN:
    *output = obj2pchar<unsigned long>(temp_service->last_time_unknown);
    break;

  case MACRO_LASTSERVICECRITICAL:
    *output = obj2pchar<unsigned long>(temp_service->last_time_critical);
    break;

  case MACRO_SERVICEDOWNTIME:
    *output = obj2pchar(temp_service->last_state);
    break;

  case MACRO_SERVICEPERCENTCHANGE:{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << temp_service->percent_state_change;
    *output = my_strdup(oss.str().c_str());
  }
    break;

  case MACRO_SERVICEDURATIONSEC:
  case MACRO_SERVICEDURATION:
    time(&current_time);
    duration = (unsigned long)(current_time - temp_service->last_state_change);

    /* get the state duration in seconds */
    if (macro_type == MACRO_SERVICEDURATIONSEC) {
      std::ostringstream oss;
      oss << duration;
      *output = my_strdup(oss.str().c_str());
    }
    /* get the state duration */
    else {
      days = duration / 86400;
      duration -= (days * 86400);
      hours = duration / 3600;
      duration -= (hours * 3600);
      minutes = duration / 60;
      duration -= (minutes * 60);
      seconds = duration;
      std::ostringstream oss;
      oss << days << "d "
	  << hours << "h "
	  << minutes << "m "
	  << seconds << "s";
      *output = my_strdup(oss.str().c_str());
    }
    break;

  case MACRO_SERVICENOTIFICATIONNUMBER:
    *output = obj2pchar(temp_service->last_state);
    break;

  case MACRO_SERVICENOTIFICATIONID:
    *output = obj2pchar(temp_service->current_notification_id);
    break;

  case MACRO_SERVICEEVENTID:
    *output = obj2pchar(temp_service->current_event_id);
    break;

  case MACRO_LASTSERVICEEVENTID:
    *output = obj2pchar(temp_service->last_event_id);
    break;

  case MACRO_SERVICEPROBLEMID:
    *output = obj2pchar(temp_service->current_problem_id);
    break;

  case MACRO_LASTSERVICEPROBLEMID:
    *output = obj2pchar(temp_service->last_problem_id);
    break;

  case MACRO_SERVICEACTIONURL:
    if (temp_service->action_url)
      *output = my_strdup(temp_service->action_url);
    break;

  case MACRO_SERVICENOTESURL:
    if (temp_service->notes_url)
      *output = my_strdup(temp_service->notes_url);
    break;

  case MACRO_SERVICENOTES:
    if (temp_service->notes)
      *output = my_strdup(temp_service->notes);
    break;

  case MACRO_SERVICEGROUPNAMES:
    /* find all servicegroups this service is associated with */
    for (temp_objectlist = temp_service->servicegroups_ptr;
         temp_objectlist != NULL;
         temp_objectlist = temp_objectlist->next) {

      if ((temp_servicegroup = (servicegroup*)temp_objectlist->object_ptr) == NULL)
        continue;

      std::ostringstream oss;
      if (buf2)
        oss << buf2 << ',';
      oss << temp_servicegroup->group_name;
      buf1 = my_strdup(oss.str().c_str());
      delete[] buf2;
      buf2 = buf1;
    }
    if (buf2) {
      *output = my_strdup(buf2);
      delete[] buf2;
      buf2 = NULL;
    }
    break;

    /***************/
    /* MISC MACROS */
    /***************/
  case MACRO_SERVICEACKAUTHOR:
  case MACRO_SERVICEACKAUTHORNAME:
  case MACRO_SERVICEACKAUTHORALIAS:
  case MACRO_SERVICEACKCOMMENT:
    /* no need to do any more work - these are already precomputed elsewhere */
    /* NOTE: these macros won't work as on-demand macros */
    *output = mac->x[macro_type];
    *free_macro = FALSE;
    break;

  default:
    log_debug_info(DEBUGL_MACROS, 0,
                   "UNHANDLED SERVICE MACRO #%d! THIS IS A BUG!\n",
                   macro_type);
    return (ERROR);
  }

  /* post-processing */
  /* notes, notes URL and action URL macros may themselves contain macros, so process them... */
  switch (macro_type) {
  case MACRO_SERVICEACTIONURL:
  case MACRO_SERVICENOTESURL:
    process_macros_r(mac, *output, &temp_buffer,
                     URL_ENCODE_MACRO_CHARS);
    delete[] *output;
    *output = temp_buffer;
    break;

  case MACRO_SERVICENOTES:
    process_macros_r(mac, *output, &temp_buffer, 0);
    delete[] *output;
    *output = temp_buffer;
    break;

  default:
    break;
  }
  return (OK);
}

/* computes a servicegroup macro */
int grab_standard_servicegroup_macro(nagios_macros* mac,
                                     int macro_type,
                                     servicegroup* temp_servicegroup,
                                     char** output) {
  servicesmember* temp_servicesmember = NULL;
  char* temp_buffer = NULL;
  unsigned int temp_len = 0;
  unsigned int init_len = 0;

  if (temp_servicegroup == NULL || output == NULL)
    return (ERROR);

  /* get the macro value */
  switch (macro_type) {
  case MACRO_SERVICEGROUPNAME:
    *output = my_strdup(temp_servicegroup->group_name);
    break;

  case MACRO_SERVICEGROUPALIAS:
    if (temp_servicegroup->alias)
      *output = my_strdup(temp_servicegroup->alias);
    break;

  case MACRO_SERVICEGROUPMEMBERS:
    /* make the calculations for total string length */
    for (temp_servicesmember = temp_servicegroup->members;
         temp_servicesmember != NULL;
         temp_servicesmember = temp_servicesmember->next) {
      if (temp_servicesmember->host_name == NULL
          || temp_servicesmember->service_description == NULL)
        continue;
      if (temp_len == 0) {
        temp_len +=
          strlen(temp_servicesmember->host_name) +
          strlen(temp_servicesmember->service_description) + 2;
      }
      else {
        temp_len +=
          strlen(temp_servicesmember->host_name) +
          strlen(temp_servicesmember->service_description) + 3;
      }
    }
    /* allocate or reallocate the memory buffer */
    if (*output == NULL)
      *output = new char[temp_len];
    else {
      init_len = strlen(*output);
      temp_len += init_len;
      *output = resize_string(*output, temp_len);
    }
    /* now fill in the string with the group members */
    for (temp_servicesmember = temp_servicegroup->members;
         temp_servicesmember != NULL;
         temp_servicesmember = temp_servicesmember->next) {
      if (temp_servicesmember->host_name == NULL
          || temp_servicesmember->service_description == NULL)
        continue;
      temp_buffer = *output + init_len;
      if (init_len == 0)      /* If our buffer didn't contain anything, we just need to write "%s,%s" */
        init_len += sprintf(temp_buffer, "%s,%s",
			    temp_servicesmember->host_name,
			    temp_servicesmember->service_description);
      else                    /* Now we need to write ",%s,%s" */
        init_len += sprintf(temp_buffer, ",%s,%s",
			    temp_servicesmember->host_name,
			    temp_servicesmember->service_description);
    }
    break;
  case MACRO_SERVICEGROUPACTIONURL:
    if (temp_servicegroup->action_url)
      *output = my_strdup(temp_servicegroup->action_url);
    break;

  case MACRO_SERVICEGROUPNOTESURL:
    if (temp_servicegroup->notes_url)
      *output = my_strdup(temp_servicegroup->notes_url);
    break;

  case MACRO_SERVICEGROUPNOTES:
    if (temp_servicegroup->notes)
      *output = my_strdup(temp_servicegroup->notes);
    break;

  default:
    log_debug_info(DEBUGL_MACROS, 0,
                   "UNHANDLED SERVICEGROUP MACRO #%d! THIS IS A BUG!\n",
                   macro_type);
    return (ERROR);
  }

  /* post-processing */
  /* notes, notes URL and action URL macros may themselves contain macros, so process them... */
  switch (macro_type) {
  case MACRO_SERVICEGROUPACTIONURL:
  case MACRO_SERVICEGROUPNOTESURL:
    process_macros_r(mac, *output, &temp_buffer, URL_ENCODE_MACRO_CHARS);
    delete[] *output;
    *output = temp_buffer;
    break;

  case MACRO_SERVICEGROUPNOTES:
    process_macros_r(mac, *output, &temp_buffer, 0);
    delete[] *output;
    *output = temp_buffer;
    break;

  default:
    break;
  }
  return (OK);
}

/* computes a contact macro */
int grab_standard_contact_macro(nagios_macros* mac,
				int macro_type,
                                contact* temp_contact,
				char** output) {
  contactgroup* temp_contactgroup = NULL;
  objectlist* temp_objectlist = NULL;
  char* buf1 = NULL;
  char* buf2 = NULL;

  (void)mac;

  if (temp_contact == NULL || output == NULL)
    return (ERROR);

  /* get the macro value */
  switch (macro_type) {
  case MACRO_CONTACTNAME:
    *output = my_strdup(temp_contact->name);
    break;

  case MACRO_CONTACTALIAS:
    *output = my_strdup(temp_contact->alias);
    break;

  case MACRO_CONTACTEMAIL:
    if (temp_contact->email)
      *output = my_strdup(temp_contact->email);
    break;

  case MACRO_CONTACTPAGER:
    if (temp_contact->pager)
      *output = my_strdup(temp_contact->pager);
    break;

  case MACRO_CONTACTGROUPNAMES:
    /* get the contactgroup names */
    /* find all contactgroups this contact is a member of */
    for (temp_objectlist = temp_contact->contactgroups_ptr;
         temp_objectlist != NULL;
         temp_objectlist = temp_objectlist->next) {

      if ((temp_contactgroup = (contactgroup*)temp_objectlist->object_ptr) == NULL)
        continue;

      std::ostringstream oss;
      if (buf2)
        oss << buf2 << ',';
      oss << temp_contactgroup->group_name;
      buf1 = my_strdup(oss.str().c_str());
      delete[] buf2;
      buf2 = buf1;
    }
    if (buf2) {
      *output = my_strdup(buf2);
      delete[] buf2;
      buf2 = NULL;
    }
    break;

  default:
    log_debug_info(DEBUGL_MACROS, 0,
                   "UNHANDLED CONTACT MACRO #%d! THIS IS A BUG!\n",
                   macro_type);
    return (ERROR);
  }
  return (OK);
}

/* computes a contact address macro */
int grab_contact_address_macro(unsigned int macro_num,
                               contact* temp_contact,
			       char** output) {
  if (macro_num >= MAX_CONTACT_ADDRESSES)
    return (ERROR);

  if (temp_contact == NULL || output == NULL)
    return (ERROR);

  /* get the macro */
  if (temp_contact->address[macro_num])
    *output = my_strdup(temp_contact->address[macro_num]);
  return (OK);
}

/* computes a contactgroup macro */
int grab_standard_contactgroup_macro(int macro_type,
                                     contactgroup* temp_contactgroup,
                                     char** output) {
  contactsmember* temp_contactsmember = NULL;

  if (temp_contactgroup == NULL || output == NULL)
    return (ERROR);

  /* get the macro value */
  switch (macro_type) {
  case MACRO_CONTACTGROUPNAME:
    *output = my_strdup(temp_contactgroup->group_name);
    break;

  case MACRO_CONTACTGROUPALIAS:
    if (temp_contactgroup->alias)
      *output = my_strdup(temp_contactgroup->alias);
    break;

  case MACRO_CONTACTGROUPMEMBERS:
    /* get the member list */
    for (temp_contactsmember = temp_contactgroup->members;
         temp_contactsmember != NULL;
         temp_contactsmember = temp_contactsmember->next) {
      if (temp_contactsmember->contact_name == NULL)
        continue;
      if (*output == NULL)
        *output = my_strdup(temp_contactsmember->contact_name);
      else {
        *output = resize_string(*output,
				strlen(*output) + strlen(temp_contactsmember->contact_name) + 2);
        strcat(*output, ",");
        strcat(*output, temp_contactsmember->contact_name);
      }
    }
    break;

  default:
    log_debug_info(DEBUGL_MACROS, 0,
                   "UNHANDLED CONTACTGROUP MACRO #%d! THIS IS A BUG!\n",
                   macro_type);
    return (ERROR);
  }
  return (OK);
}

/* computes a custom object macro */
int grab_custom_object_macro(nagios_macros* mac,
			     char* macro_name,
                             customvariablesmember* vars,
                             char** output) {
  customvariablesmember* temp_customvariablesmember = NULL;
  int result = ERROR;

  (void)mac;

  if (macro_name == NULL || vars == NULL || output == NULL)
    return (ERROR);

  /* get the custom variable */
  for (temp_customvariablesmember = vars;
       temp_customvariablesmember != NULL;
       temp_customvariablesmember = temp_customvariablesmember->next) {

    if (temp_customvariablesmember->variable_name == NULL)
      continue;

    if (!strcmp(macro_name, temp_customvariablesmember->variable_name)) {
      if (temp_customvariablesmember->variable_value)
        *output = my_strdup(temp_customvariablesmember->variable_value);
      result = OK;
      break;
    }
  }
  return (result);
}

/******************************************************************/
/********************* MACRO STRING FUNCTIONS *********************/
/******************************************************************/

/* cleans illegal characters in macros before output */
char const* clean_macro_chars(char* macro, int options) {
  int x = 0;
  int y = 0;
  int z = 0;
  int ch = 0;
  int len = 0;
  int illegal_char = 0;

  if (macro == NULL)
    return ("");

  len = (int)strlen(macro);

  /* strip illegal characters out of macro */
  if (options & STRIP_ILLEGAL_MACRO_CHARS) {
    for (y = 0, x = 0; x < len; x++) {
      /*ch=(int)macro[x]; */
      /* allow non-ASCII characters (Japanese, etc) */
      ch = macro[x] & 0xff;

      /* illegal ASCII characters */
      if (ch < 32 || ch == 127)
        continue;

      /* illegal user-specified characters */
      illegal_char = FALSE;
      char const* illegal_output_chars = config.get_illegal_output_chars().toStdString().c_str();
      if (illegal_output_chars != NULL) {
        for (z = 0; illegal_output_chars[z] != '\x0'; z++) {
          if (ch == (int)illegal_output_chars[z]) {
            illegal_char = TRUE;
            break;
          }
        }
      }

      if (illegal_char == FALSE)
        macro[y++] = macro[x];
    }

    macro[y++] = '\x0';
  }

#ifdef ON_HOLD_FOR_NOW
  /* escape nasty character in macro */
  if (options & ESCAPE_MACRO_CHARS) {
  }
#endif
  return (macro);
}

/* encodes a string in proper URL format */
char* get_url_encoded_string(char* input) {
  int x = 0;
  int y = 0;
  char* encoded_url_string = NULL;
  char temp_expansion[6] = "";

  /* bail if no input */
  if (input == NULL)
    return (NULL);

  /* allocate enough memory to escape all characters if necessary */
  encoded_url_string = new char[strlen(input) * 3 + 1];

  /* check/encode all characters */
  for (x = 0, y = 0; input[x] != (char)'\x0'; x++) {
    /* alpha-numeric characters and a few other characters don't get encoded */
    if (((char)input[x] >= '0' && (char)input[x] <= '9')
        || ((char)input[x] >= 'A' && (char)input[x] <= 'Z')
        || ((char)input[x] >= (char)'a' && (char)input[x] <= (char)'z')
        || (char)input[x] == (char)'.' || (char)input[x] == (char)'-'
        || (char)input[x] == (char)'_' || (char)input[x] == (char)':'
        || (char)input[x] == (char)'/' || (char)input[x] == (char)'?'
        || (char)input[x] == (char)'=' || (char)input[x] == (char)'&')
      encoded_url_string[y++] = input[x];
    /* spaces are pluses */
    else if (input[x] == ' ')
      encoded_url_string[y++] = '+';
    /* anything else gets represented by its hex value */
    else {
      encoded_url_string[y] = '\x0';
      sprintf(temp_expansion, "%%%02X", (unsigned int)(input[x] & 0xFF));
      strcat(encoded_url_string, temp_expansion);
      y += 3;
    }
  }

  /* terminate encoded string */
  encoded_url_string[y] = '\x0';
  return (encoded_url_string);
}

/******************************************************************/
/***************** MACRO INITIALIZATION FUNCTIONS *****************/
/******************************************************************/

/* initializes global macros */
int init_macros(void) {
  init_macrox_names();

  /*
   * non-volatile macros are free()'d when they're set.
   * We must do this in order to not lose the constant
   * ones when we get SIGHUP or a RESTART_PROGRAM event
   * from the command fifo. Otherwise a memset() would
   * have been better.
   */
  clear_volatile_macros(&global_macros);

  /* backwards compatibility hack */
  macro_x = global_macros.x;
  return (OK);
}

/*
 * initializes the names of macros, using this nifty little macro
 * which ensures we never add any typos to the list
 */
#define add_macrox_name(name) macro_x_names[MACRO_##name] = my_strdup(#name)
int init_macrox_names(void) {
  unsigned int x = 0;

  /* initialize macro names */
  for (x = 0; x < MACRO_X_COUNT; x++)
    macro_x_names[x] = NULL;

  /* initialize each macro name */
  add_macrox_name(HOSTNAME);
  add_macrox_name(HOSTALIAS);
  add_macrox_name(HOSTADDRESS);
  add_macrox_name(SERVICEDESC);
  add_macrox_name(SERVICESTATE);
  add_macrox_name(SERVICESTATEID);
  add_macrox_name(SERVICEATTEMPT);
  add_macrox_name(SERVICEISVOLATILE);
  add_macrox_name(LONGDATETIME);
  add_macrox_name(SHORTDATETIME);
  add_macrox_name(DATE);
  add_macrox_name(TIME);
  add_macrox_name(TIMET);
  add_macrox_name(LASTHOSTCHECK);
  add_macrox_name(LASTSERVICECHECK);
  add_macrox_name(LASTHOSTSTATECHANGE);
  add_macrox_name(LASTSERVICESTATECHANGE);
  add_macrox_name(HOSTOUTPUT);
  add_macrox_name(SERVICEOUTPUT);
  add_macrox_name(HOSTPERFDATA);
  add_macrox_name(SERVICEPERFDATA);
  add_macrox_name(CONTACTNAME);
  add_macrox_name(CONTACTALIAS);
  add_macrox_name(CONTACTEMAIL);
  add_macrox_name(CONTACTPAGER);
  add_macrox_name(ADMINEMAIL);
  add_macrox_name(ADMINPAGER);
  add_macrox_name(HOSTSTATE);
  add_macrox_name(HOSTSTATEID);
  add_macrox_name(HOSTATTEMPT);
  add_macrox_name(NOTIFICATIONTYPE);
  add_macrox_name(NOTIFICATIONNUMBER);
  add_macrox_name(NOTIFICATIONISESCALATED);
  add_macrox_name(HOSTEXECUTIONTIME);
  add_macrox_name(SERVICEEXECUTIONTIME);
  add_macrox_name(HOSTLATENCY);
  add_macrox_name(SERVICELATENCY);
  add_macrox_name(HOSTDURATION);
  add_macrox_name(SERVICEDURATION);
  add_macrox_name(HOSTDURATIONSEC);
  add_macrox_name(SERVICEDURATIONSEC);
  add_macrox_name(HOSTDOWNTIME);
  add_macrox_name(SERVICEDOWNTIME);
  add_macrox_name(HOSTSTATETYPE);
  add_macrox_name(SERVICESTATETYPE);
  add_macrox_name(HOSTPERCENTCHANGE);
  add_macrox_name(SERVICEPERCENTCHANGE);
  add_macrox_name(HOSTGROUPNAME);
  add_macrox_name(HOSTGROUPALIAS);
  add_macrox_name(SERVICEGROUPNAME);
  add_macrox_name(SERVICEGROUPALIAS);
  add_macrox_name(HOSTACKAUTHOR);
  add_macrox_name(HOSTACKCOMMENT);
  add_macrox_name(SERVICEACKAUTHOR);
  add_macrox_name(SERVICEACKCOMMENT);
  add_macrox_name(LASTSERVICEOK);
  add_macrox_name(LASTSERVICEWARNING);
  add_macrox_name(LASTSERVICEUNKNOWN);
  add_macrox_name(LASTSERVICECRITICAL);
  add_macrox_name(LASTHOSTUP);
  add_macrox_name(LASTHOSTDOWN);
  add_macrox_name(LASTHOSTUNREACHABLE);
  add_macrox_name(SERVICECHECKCOMMAND);
  add_macrox_name(HOSTCHECKCOMMAND);
  add_macrox_name(MAINCONFIGFILE);
  add_macrox_name(STATUSDATAFILE);
  add_macrox_name(HOSTDISPLAYNAME);
  add_macrox_name(SERVICEDISPLAYNAME);
  add_macrox_name(RETENTIONDATAFILE);
  add_macrox_name(OBJECTCACHEFILE);
  add_macrox_name(TEMPFILE);
  add_macrox_name(LOGFILE);
  add_macrox_name(RESOURCEFILE);
  add_macrox_name(COMMANDFILE);
  add_macrox_name(HOSTPERFDATAFILE);
  add_macrox_name(SERVICEPERFDATAFILE);
  add_macrox_name(HOSTACTIONURL);
  add_macrox_name(HOSTNOTESURL);
  add_macrox_name(HOSTNOTES);
  add_macrox_name(SERVICEACTIONURL);
  add_macrox_name(SERVICENOTESURL);
  add_macrox_name(SERVICENOTES);
  add_macrox_name(TOTALHOSTSUP);
  add_macrox_name(TOTALHOSTSDOWN);
  add_macrox_name(TOTALHOSTSUNREACHABLE);
  add_macrox_name(TOTALHOSTSDOWNUNHANDLED);
  add_macrox_name(TOTALHOSTSUNREACHABLEUNHANDLED);
  add_macrox_name(TOTALHOSTPROBLEMS);
  add_macrox_name(TOTALHOSTPROBLEMSUNHANDLED);
  add_macrox_name(TOTALSERVICESOK);
  add_macrox_name(TOTALSERVICESWARNING);
  add_macrox_name(TOTALSERVICESCRITICAL);
  add_macrox_name(TOTALSERVICESUNKNOWN);
  add_macrox_name(TOTALSERVICESWARNINGUNHANDLED);
  add_macrox_name(TOTALSERVICESCRITICALUNHANDLED);
  add_macrox_name(TOTALSERVICESUNKNOWNUNHANDLED);
  add_macrox_name(TOTALSERVICEPROBLEMS);
  add_macrox_name(TOTALSERVICEPROBLEMSUNHANDLED);
  add_macrox_name(PROCESSSTARTTIME);
  add_macrox_name(HOSTCHECKTYPE);
  add_macrox_name(SERVICECHECKTYPE);
  add_macrox_name(LONGHOSTOUTPUT);
  add_macrox_name(LONGSERVICEOUTPUT);
  add_macrox_name(TEMPPATH);
  add_macrox_name(HOSTNOTIFICATIONNUMBER);
  add_macrox_name(SERVICENOTIFICATIONNUMBER);
  add_macrox_name(HOSTNOTIFICATIONID);
  add_macrox_name(SERVICENOTIFICATIONID);
  add_macrox_name(HOSTEVENTID);
  add_macrox_name(LASTHOSTEVENTID);
  add_macrox_name(SERVICEEVENTID);
  add_macrox_name(LASTSERVICEEVENTID);
  add_macrox_name(HOSTGROUPNAMES);
  add_macrox_name(SERVICEGROUPNAMES);
  add_macrox_name(HOSTACKAUTHORNAME);
  add_macrox_name(HOSTACKAUTHORALIAS);
  add_macrox_name(SERVICEACKAUTHORNAME);
  add_macrox_name(SERVICEACKAUTHORALIAS);
  add_macrox_name(MAXHOSTATTEMPTS);
  add_macrox_name(MAXSERVICEATTEMPTS);
  add_macrox_name(TOTALHOSTSERVICES);
  add_macrox_name(TOTALHOSTSERVICESOK);
  add_macrox_name(TOTALHOSTSERVICESWARNING);
  add_macrox_name(TOTALHOSTSERVICESUNKNOWN);
  add_macrox_name(TOTALHOSTSERVICESCRITICAL);
  add_macrox_name(HOSTGROUPNOTES);
  add_macrox_name(HOSTGROUPNOTESURL);
  add_macrox_name(HOSTGROUPACTIONURL);
  add_macrox_name(SERVICEGROUPNOTES);
  add_macrox_name(SERVICEGROUPNOTESURL);
  add_macrox_name(SERVICEGROUPACTIONURL);
  add_macrox_name(HOSTGROUPMEMBERS);
  add_macrox_name(SERVICEGROUPMEMBERS);
  add_macrox_name(CONTACTGROUPNAME);
  add_macrox_name(CONTACTGROUPALIAS);
  add_macrox_name(CONTACTGROUPMEMBERS);
  add_macrox_name(CONTACTGROUPNAMES);
  add_macrox_name(NOTIFICATIONRECIPIENTS);
  add_macrox_name(NOTIFICATIONAUTHOR);
  add_macrox_name(NOTIFICATIONAUTHORNAME);
  add_macrox_name(NOTIFICATIONAUTHORALIAS);
  add_macrox_name(NOTIFICATIONCOMMENT);
  add_macrox_name(EVENTSTARTTIME);
  add_macrox_name(HOSTPROBLEMID);
  add_macrox_name(LASTHOSTPROBLEMID);
  add_macrox_name(SERVICEPROBLEMID);
  add_macrox_name(LASTSERVICEPROBLEMID);
  add_macrox_name(ISVALIDTIME);
  add_macrox_name(NEXTVALIDTIME);
  add_macrox_name(LASTHOSTSTATE);
  add_macrox_name(LASTHOSTSTATEID);
  add_macrox_name(LASTSERVICESTATE);
  add_macrox_name(LASTSERVICESTATEID);

  return (OK);
}

/******************************************************************/
/********************* MACRO CLEANUP FUNCTIONS ********************/
/******************************************************************/

/* free memory associated with the macrox names */
int free_macrox_names(void) {
  unsigned int x = 0;

  /* free each macro name */
  for (x = 0; x < MACRO_X_COUNT; x++) {
    delete[] macro_x_names[x];
    macro_x_names[x] = NULL;
  }
  return (OK);
}

/* clear argv macros - used in commands */
int clear_argv_macros(nagios_macros* mac) {
  unsigned int x = 0;

  /* command argument macros */
  for (x = 0; x < MAX_COMMAND_ARGUMENTS; x++) {
    delete[] mac->argv[x];
    mac->argv[x] = NULL;
  }
  return (OK);
}

/*
 * copies non-volatile macros from global macro_x to **dest, which
 * must be large enough to hold at least MACRO_X_COUNT entries.
 * We use a shortlived macro to save up on typing
 */
#define cp_macro(name) dest[MACRO_##name] = global_macros.x[MACRO_##name]
void copy_constant_macros(char** dest) {
  cp_macro(ADMINEMAIL);
  cp_macro(ADMINPAGER);
  cp_macro(MAINCONFIGFILE);
  cp_macro(STATUSDATAFILE);
  cp_macro(RETENTIONDATAFILE);
  cp_macro(OBJECTCACHEFILE);
  cp_macro(TEMPFILE);
  cp_macro(LOGFILE);
  cp_macro(RESOURCEFILE);
  cp_macro(COMMANDFILE);
  cp_macro(HOSTPERFDATAFILE);
  cp_macro(SERVICEPERFDATAFILE);
  cp_macro(PROCESSSTARTTIME);
  cp_macro(TEMPPATH);
  cp_macro(EVENTSTARTTIME);
}

#undef cp_macro

/* clear all macros that are not "constant" (i.e. they change throughout the course of monitoring) */
int clear_volatile_macros(nagios_macros* mac) {
  customvariablesmember* this_customvariablesmember = NULL;
  customvariablesmember* next_customvariablesmember = NULL;
  unsigned int x = 0;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {

    case MACRO_ADMINEMAIL:
    case MACRO_ADMINPAGER:
    case MACRO_MAINCONFIGFILE:
    case MACRO_STATUSDATAFILE:
    case MACRO_RETENTIONDATAFILE:
    case MACRO_OBJECTCACHEFILE:
    case MACRO_TEMPFILE:
    case MACRO_LOGFILE:
    case MACRO_RESOURCEFILE:
    case MACRO_COMMANDFILE:
    case MACRO_HOSTPERFDATAFILE:
    case MACRO_SERVICEPERFDATAFILE:
    case MACRO_PROCESSSTARTTIME:
    case MACRO_TEMPPATH:
    case MACRO_EVENTSTARTTIME:
      /* these don't change during the course of monitoring, so no need to free them */
      break;

    default:
      delete[] mac->x[x];
      mac->x[x] = NULL;
      break;
    }
  }

  /* contact address macros */
  for (x = 0; x < MAX_CONTACT_ADDRESSES; x++) {
    delete[] mac->contactaddress[x];
    mac->contactaddress[x] = NULL;
  }

  /* clear macro pointers */
  mac->host_ptr = NULL;
  mac->hostgroup_ptr = NULL;
  mac->service_ptr = NULL;
  mac->servicegroup_ptr = NULL;
  mac->contact_ptr = NULL;
  mac->contactgroup_ptr = NULL;

  /* clear on-demand macro */
  delete[] mac->ondemand;
  mac->ondemand = NULL;

  /* clear ARGx macros */
  clear_argv_macros(mac);

  /* clear custom host variables */
  for (this_customvariablesmember = mac->custom_host_vars;
       this_customvariablesmember != NULL;
       this_customvariablesmember = next_customvariablesmember) {
    next_customvariablesmember = this_customvariablesmember->next;
    delete[] this_customvariablesmember->variable_name;
    delete[] this_customvariablesmember->variable_value;
    delete this_customvariablesmember;
  }
  mac->custom_host_vars = NULL;

  /* clear custom service variables */
  for (this_customvariablesmember = mac->custom_service_vars;
       this_customvariablesmember != NULL;
       this_customvariablesmember = next_customvariablesmember) {
    next_customvariablesmember = this_customvariablesmember->next;
    delete[] this_customvariablesmember->variable_name;
    delete[] this_customvariablesmember->variable_value;
    delete this_customvariablesmember;
  }
  mac->custom_service_vars = NULL;

  /* clear custom contact variables */
  for (this_customvariablesmember = mac->custom_contact_vars;
       this_customvariablesmember != NULL;
       this_customvariablesmember = next_customvariablesmember) {
    next_customvariablesmember = this_customvariablesmember->next;
    delete[] this_customvariablesmember->variable_name;
    delete[] this_customvariablesmember->variable_value;
    delete this_customvariablesmember;
  }
  mac->custom_contact_vars = NULL;

  return (OK);
}

/* clear service macros */
int clear_service_macros(nagios_macros* mac) {
  unsigned int x;
  customvariablesmember* this_customvariablesmember = NULL;
  customvariablesmember* next_customvariablesmember = NULL;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {
    case MACRO_SERVICEDESC:
    case MACRO_SERVICEDISPLAYNAME:
    case MACRO_SERVICEOUTPUT:
    case MACRO_LONGSERVICEOUTPUT:
    case MACRO_SERVICEPERFDATA:
    case MACRO_SERVICECHECKCOMMAND:
    case MACRO_SERVICECHECKTYPE:
    case MACRO_SERVICESTATETYPE:
    case MACRO_SERVICESTATE:
    case MACRO_SERVICEISVOLATILE:
    case MACRO_SERVICESTATEID:
    case MACRO_SERVICEATTEMPT:
    case MACRO_MAXSERVICEATTEMPTS:
    case MACRO_SERVICEEXECUTIONTIME:
    case MACRO_SERVICELATENCY:
    case MACRO_LASTSERVICECHECK:
    case MACRO_LASTSERVICESTATECHANGE:
    case MACRO_LASTSERVICEOK:
    case MACRO_LASTSERVICEWARNING:
    case MACRO_LASTSERVICEUNKNOWN:
    case MACRO_LASTSERVICECRITICAL:
    case MACRO_SERVICEDOWNTIME:
    case MACRO_SERVICEPERCENTCHANGE:
    case MACRO_SERVICEDURATIONSEC:
    case MACRO_SERVICEDURATION:
    case MACRO_SERVICENOTIFICATIONNUMBER:
    case MACRO_SERVICENOTIFICATIONID:
    case MACRO_SERVICEEVENTID:
    case MACRO_LASTSERVICEEVENTID:
    case MACRO_SERVICEACTIONURL:
    case MACRO_SERVICENOTESURL:
    case MACRO_SERVICENOTES:
    case MACRO_SERVICEGROUPNAMES:
    case MACRO_SERVICEPROBLEMID:
    case MACRO_LASTSERVICEPROBLEMID:
      delete[] mac->x[x];
      mac->x[x] = NULL;
      break;

    default:
      break;
    }
  }

  /* clear custom service variables */
  for (this_customvariablesmember = mac->custom_service_vars;
       this_customvariablesmember != NULL;
       this_customvariablesmember = next_customvariablesmember) {
    next_customvariablesmember = this_customvariablesmember->next;
    delete[] this_customvariablesmember->variable_name;
    delete[] this_customvariablesmember->variable_value;
    delete this_customvariablesmember;
  }
  mac->custom_service_vars = NULL;

  /* clear pointers */
  mac->service_ptr = NULL;

  return (OK);
}

/* clear host macros */
int clear_host_macros(nagios_macros* mac) {
  unsigned int x;
  customvariablesmember* this_customvariablesmember = NULL;
  customvariablesmember* next_customvariablesmember = NULL;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {
    case MACRO_HOSTNAME:
    case MACRO_HOSTDISPLAYNAME:
    case MACRO_HOSTALIAS:
    case MACRO_HOSTADDRESS:
    case MACRO_HOSTSTATE:
    case MACRO_HOSTSTATEID:
    case MACRO_HOSTCHECKTYPE:
    case MACRO_HOSTSTATETYPE:
    case MACRO_HOSTOUTPUT:
    case MACRO_LONGHOSTOUTPUT:
    case MACRO_HOSTPERFDATA:
    case MACRO_HOSTCHECKCOMMAND:
    case MACRO_HOSTATTEMPT:
    case MACRO_MAXHOSTATTEMPTS:
    case MACRO_HOSTDOWNTIME:
    case MACRO_HOSTPERCENTCHANGE:
    case MACRO_HOSTDURATIONSEC:
    case MACRO_HOSTDURATION:
    case MACRO_HOSTEXECUTIONTIME:
    case MACRO_HOSTLATENCY:
    case MACRO_LASTHOSTCHECK:
    case MACRO_LASTHOSTSTATECHANGE:
    case MACRO_LASTHOSTUP:
    case MACRO_LASTHOSTDOWN:
    case MACRO_LASTHOSTUNREACHABLE:
    case MACRO_HOSTNOTIFICATIONNUMBER:
    case MACRO_HOSTNOTIFICATIONID:
    case MACRO_HOSTEVENTID:
    case MACRO_LASTHOSTEVENTID:
    case MACRO_HOSTACTIONURL:
    case MACRO_HOSTNOTESURL:
    case MACRO_HOSTNOTES:
    case MACRO_HOSTGROUPNAMES:
    case MACRO_TOTALHOSTSERVICES:
    case MACRO_TOTALHOSTSERVICESOK:
    case MACRO_TOTALHOSTSERVICESWARNING:
    case MACRO_TOTALHOSTSERVICESUNKNOWN:
    case MACRO_TOTALHOSTSERVICESCRITICAL:
    case MACRO_HOSTPROBLEMID:
    case MACRO_LASTHOSTPROBLEMID:
      delete[] mac->x[x];
      mac->x[x] = NULL;
      break;

    default:
      break;
    }
  }

  /* clear custom host variables */
  for (this_customvariablesmember = mac->custom_host_vars;
       this_customvariablesmember != NULL;
       this_customvariablesmember = next_customvariablesmember) {
    next_customvariablesmember = this_customvariablesmember->next;
    delete[] this_customvariablesmember->variable_name;
    delete[] this_customvariablesmember->variable_value;
    delete this_customvariablesmember;
  }
  mac->custom_host_vars = NULL;

  /* clear pointers */
  mac->host_ptr = NULL;

  return (OK);
}

/* clear hostgroup macros */
int clear_hostgroup_macros(nagios_macros* mac) {
  unsigned int x;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {
    case MACRO_HOSTGROUPNAME:
    case MACRO_HOSTGROUPALIAS:
    case MACRO_HOSTGROUPMEMBERS:
    case MACRO_HOSTGROUPACTIONURL:
    case MACRO_HOSTGROUPNOTESURL:
    case MACRO_HOSTGROUPNOTES:
      delete[] mac->x[x];
      mac->x[x] = NULL;
      break;

    default:
      break;
    }
  }

  /* clear pointers */
  mac->hostgroup_ptr = NULL;

  return (OK);
}

/* clear servicegroup macros */
int clear_servicegroup_macros(nagios_macros* mac) {
  unsigned int x;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {
    case MACRO_SERVICEGROUPNAME:
    case MACRO_SERVICEGROUPALIAS:
    case MACRO_SERVICEGROUPMEMBERS:
    case MACRO_SERVICEGROUPACTIONURL:
    case MACRO_SERVICEGROUPNOTESURL:
    case MACRO_SERVICEGROUPNOTES:
      delete[] mac->x[x];
      mac->x[x] = NULL;
      break;

    default:
      break;
    }
  }

  /* clear pointers */
  mac->servicegroup_ptr = NULL;

  return (OK);
}

/* clear contact macros */
int clear_contact_macros(nagios_macros* mac) {
  unsigned int x;
  customvariablesmember* this_customvariablesmember = NULL;
  customvariablesmember* next_customvariablesmember = NULL;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {
    case MACRO_CONTACTNAME:
    case MACRO_CONTACTALIAS:
    case MACRO_CONTACTEMAIL:
    case MACRO_CONTACTPAGER:
    case MACRO_CONTACTGROUPNAMES:
      delete[] mac->x[x];
      mac->x[x] = NULL;
      break;

    default:
      break;
    }
  }

  /* clear contact addresses */
  for (x = 0; x < MAX_CONTACT_ADDRESSES; x++) {
    delete[] mac->contactaddress[x];
    mac->contactaddress[x] = NULL;
  }

  /* clear custom contact variables */
  for (this_customvariablesmember = mac->custom_contact_vars;
       this_customvariablesmember != NULL;
       this_customvariablesmember = next_customvariablesmember) {
    next_customvariablesmember = this_customvariablesmember->next;
    delete[] this_customvariablesmember->variable_name;
    delete[] this_customvariablesmember->variable_value;
    delete this_customvariablesmember;
  }
  mac->custom_contact_vars = NULL;

  /* clear pointers */
  mac->contact_ptr = NULL;

  return (OK);
}

/* clear contactgroup macros */
int clear_contactgroup_macros(nagios_macros* mac) {
  unsigned int x;

  for (x = 0; x < MACRO_X_COUNT; x++) {
    switch (x) {
    case MACRO_CONTACTGROUPNAME:
    case MACRO_CONTACTGROUPALIAS:
    case MACRO_CONTACTGROUPMEMBERS:
      delete[] mac->x[x];
      mac->x[x] = NULL;
      break;

    default:
      break;
    }
  }

  /* clear pointers */
  mac->contactgroup_ptr = NULL;

  return (OK);
}

/* clear summary macros */
int clear_summary_macros(nagios_macros* mac) {
  unsigned int x;

  for (x = MACRO_TOTALHOSTSUP;
       x <= MACRO_TOTALSERVICEPROBLEMSUNHANDLED;
       x++) {
    delete[] mac->x[x];
    mac->x[x] = NULL;
  }

  return (OK);
}

/******************************************************************/
/****************** ENVIRONMENT MACRO FUNCTIONS *******************/
/******************************************************************/

/* sets or unsets all macro environment variables */
int set_all_macro_environment_vars(nagios_macros* mac, int set) {
  if (config.get_enable_environment_macros() == false)
    return (ERROR);

  set_macrox_environment_vars(mac, set);
  set_argv_macro_environment_vars(mac, set);
  set_custom_macro_environment_vars(mac, set);
  set_contact_address_environment_vars(mac, set);

  return (OK);
}

/* sets or unsets macrox environment variables */
int set_macrox_environment_vars(nagios_macros* mac, int set) {
  unsigned int x = 0;
  int free_macro = FALSE;
  int generate_macro = TRUE;

  /* set each of the macrox environment variables */
  for (x = 0; x < MACRO_X_COUNT; x++) {
    free_macro = FALSE;

    /* generate the macro value if it hasn't already been done */
    /* THIS IS EXPENSIVE */
    if (set == TRUE) {
      generate_macro = TRUE;

      /* skip summary macro generation if lage installation tweaks are enabled */
      if ((x >= MACRO_TOTALHOSTSUP
           && x <= MACRO_TOTALSERVICEPROBLEMSUNHANDLED)
          && config.get_use_large_installation_tweaks() == true)
        generate_macro = FALSE;

      if (mac->x[x] == NULL && generate_macro == TRUE)
        grab_macrox_value(mac, x, NULL, NULL, &mac->x[x], &free_macro);
    }

    /* set the value */
    set_macro_environment_var(macro_x_names[x], mac->x[x], set);
  }

  return (OK);
}

/* sets or unsets argv macro environment variables */
int set_argv_macro_environment_vars(nagios_macros* mac, int set) {
  unsigned int x = 0;

  /* set each of the argv macro environment variables */
  for (x = 0; x < MAX_COMMAND_ARGUMENTS; x++) {
    std::ostringstream oss;
    oss << "ARG" << x + 1;
    set_macro_environment_var(oss.str().c_str(), mac->argv[x], set);
  }

  return (OK);
}

/* sets or unsets custom host/service/contact macro environment variables */
int set_custom_macro_environment_vars(nagios_macros* mac, int set) {
  customvariablesmember* temp_customvariablesmember = NULL;
  host* temp_host = NULL;
  service* temp_service = NULL;
  contact* temp_contact = NULL;

  /***** CUSTOM HOST VARIABLES *****/
  /* generate variables and save them for later */
  if ((temp_host = mac->host_ptr) && set == TRUE) {
    for (temp_customvariablesmember = temp_host->custom_variables;
         temp_customvariablesmember != NULL;
         temp_customvariablesmember = temp_customvariablesmember->next) {
      std::ostringstream oss;
      oss << "_HOST" << temp_customvariablesmember->variable_name;
      add_custom_variable_to_object(&mac->custom_host_vars,
                                    oss.str().c_str(),
                                    temp_customvariablesmember->variable_value);
    }
  }
  /* set variables */
  for (temp_customvariablesmember = mac->custom_host_vars;
       temp_customvariablesmember != NULL;
       temp_customvariablesmember = temp_customvariablesmember->next) {
    set_macro_environment_var(temp_customvariablesmember->variable_name,
                              clean_macro_chars(temp_customvariablesmember->variable_value,
						STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS),
			      set);
  }

  /***** CUSTOM SERVICE VARIABLES *****/
  /* generate variables and save them for later */
  if ((temp_service = mac->service_ptr) && set == TRUE) {
    for (temp_customvariablesmember = temp_service->custom_variables;
         temp_customvariablesmember != NULL;
         temp_customvariablesmember = temp_customvariablesmember->next) {
      std::ostringstream oss;
      oss << "_SERVICE" << temp_customvariablesmember->variable_name;
      add_custom_variable_to_object(&mac->custom_service_vars,
                                    oss.str().c_str(),
                                    temp_customvariablesmember->variable_value);
    }
  }
  /* set variables */
  for (temp_customvariablesmember = mac->custom_service_vars;
       temp_customvariablesmember != NULL;
       temp_customvariablesmember = temp_customvariablesmember->next)
    set_macro_environment_var(temp_customvariablesmember->variable_name,
                              clean_macro_chars(temp_customvariablesmember->variable_value,
						STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS),
			      set);

  /***** CUSTOM CONTACT VARIABLES *****/
  /* generate variables and save them for later */
  if ((temp_contact = mac->contact_ptr) && set == TRUE) {
    for (temp_customvariablesmember = temp_contact->custom_variables;
         temp_customvariablesmember != NULL;
         temp_customvariablesmember = temp_customvariablesmember->next) {
      std::ostringstream oss;
      oss << "_CONTACT" << temp_customvariablesmember->variable_name;
      add_custom_variable_to_object(&mac->custom_contact_vars,
                                    oss.str().c_str(),
                                    temp_customvariablesmember->variable_value);
    }
  }
  /* set variables */
  for (temp_customvariablesmember = mac->custom_contact_vars;
       temp_customvariablesmember != NULL;
       temp_customvariablesmember = temp_customvariablesmember->next)
    set_macro_environment_var(temp_customvariablesmember->variable_name,
                              clean_macro_chars(temp_customvariablesmember->variable_value,
						STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS),
			      set);

  return (OK);
}

/* sets or unsets contact address environment variables */
int set_contact_address_environment_vars(nagios_macros* mac, int set) {
  unsigned int x;

  /* these only get set during notifications */
  if (mac->contact_ptr == NULL)
    return (OK);

  for (x = 0; x < MAX_CONTACT_ADDRESSES; x++) {
    std::ostringstream oss;
    oss << "CONTACTADDRESS" << x;
    set_macro_environment_var(oss.str().c_str(),
                              mac->contact_ptr->address[x],
			      set);
  }

  return (OK);
}

/* sets or unsets a macro environment variable */
int set_macro_environment_var(char const* name, char const* value, int set) {
  /* we won't mess with null variable names */
  if (name == NULL)
    return (ERROR);

  /* create environment var name */
  std::ostringstream oss;
  oss << MACRO_ENV_VAR_PREFIX << name;

  /* set or unset the environment variable */
  set_environment_var(oss.str().c_str(), value, set);

  return (OK);
}
