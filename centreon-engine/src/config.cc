/*
** Copyright 1999-2008 Ethan Galstad
** Copyright 2011-2012 Merethis
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

#include <errno.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include "com/centreon/engine/config.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/notifications.hh"
#include "com/centreon/engine/utils.hh"

using namespace com::centreon::engine::logging;

/******************************************************************/
/************** CONFIGURATION INPUT FUNCTIONS *********************/
/******************************************************************/

/* read all configuration data */
int read_all_object_data(char* main_config_file) {
  int result = OK;
  int options = 0;
  int cache = FALSE;
  int precache = FALSE;

  options = READ_ALL_OBJECT_DATA;

  /* cache object definitions if we're up and running */
  if (verify_config == FALSE && test_scheduling == FALSE)
    cache = TRUE;

  /* precache object definitions */
  if (precache_objects == TRUE
      && (verify_config == TRUE || test_scheduling == TRUE))
    precache = TRUE;

  /* read in all host configuration data from external sources */
  result = read_object_config_data(main_config_file, options, cache, precache);
  if (result != OK)
    return (ERROR);

  return (OK);
}

/* processes macros in resource file */
int read_resource_file(char* resource_file) {
  char* input = NULL;
  char* variable = NULL;
  char* value = NULL;
  char* temp_ptr = NULL;
  mmapfile* thefile = NULL;
  int current_line = 1;
  int error = FALSE;
  unsigned int user_index = 0;

  if ((thefile = mmap_fopen(resource_file)) == NULL) {
    logger(log_config_error, basic)
      << "Error: Cannot open resource file '"
      << resource_file << "' for reading!";
    return (ERROR);
  }

  /* process all lines in the resource file */
  while (1) {

    /* free memory */
    delete[] input;
    delete[] variable;
    delete[] value;

    /* read the next line */
    if ((input = mmap_fgets_multiline(thefile)) == NULL)
      break;

    current_line = thefile->current_line;

    /* skip blank lines and comments */
    if (input[0] == '#' || input[0] == '\x0' || input[0] == '\n' || input[0] == '\r')
      continue;

    strip(input);

    /* get the variable name */
    if ((temp_ptr = my_strtok(input, "=")) == NULL) {
      logger(log_config_error, basic)
        << "Error: NULL variable - Line " << current_line
        << " of resource file '" << resource_file << "'";
      error = TRUE;
      break;
    }
    variable = my_strdup(temp_ptr);

    /* get the value */
    if ((temp_ptr = my_strtok(NULL, "\n")) == NULL) {
      logger(log_config_error, basic)
        << "Error: NULL variable value - Line " << current_line
        << " of resource file '" << resource_file << "'",
      error = TRUE;
      break;
    }
    value = my_strdup(temp_ptr);

    /* what should we do with the variable/value pair? */

    /* check for macro declarations */
    if (variable[0] == '$' && variable[strlen(variable) - 1] == '$') {

      /* $USERx$ macro declarations */
      if (strstr(variable, "$USER") == variable && strlen(variable) > 5) {
        user_index = atoi(variable + 5) - 1;
        if (user_index < MAX_USER_MACROS) {
          delete[] macro_user[user_index];
          macro_user[user_index] = my_strdup(value);
        }
      }
    }
  }

  /* free leftover memory and close the file */
  delete[] input;
  mmap_fclose(thefile);

  /* free memory */
  delete[] variable;
  delete[] value;

  if (error == TRUE)
    return (ERROR);

  return (OK);
}

/****************************************************************/
/**************** CONFIG VERIFICATION FUNCTIONS *****************/
/****************************************************************/

/* do a pre-flight check to make sure object relationships, etc. make sense */
int pre_flight_check(void) {
  host* temp_host = NULL;
  char* buf = NULL;
  service* temp_service = NULL;
  command* temp_command = NULL;
  char* temp_command_name = NULL;
  int warnings = 0;
  int errors = 0;
  struct timeval tv[4];
  double runtime[4];

  if (test_scheduling == TRUE)
    gettimeofday(&tv[0], NULL);

  /********************************************/
  /* check object relationships               */
  /********************************************/
  pre_flight_object_check(&warnings, &errors);
  if (test_scheduling == TRUE)
    gettimeofday(&tv[1], NULL);

  /********************************************/
  /* check for circular paths between hosts   */
  /********************************************/
  pre_flight_circular_check(&warnings, &errors);
  if (test_scheduling == TRUE)
    gettimeofday(&tv[2], NULL);

  /********************************************/
  /* check global event handler commands...   */
  /********************************************/
  if (verify_config == TRUE)
    printf("Checking global event handlers...\n");

  if (config.get_global_host_event_handler() != "") {

    /* check the event handler command */
    buf = my_strdup(qPrintable(config.get_global_host_event_handler()));

    /* get the command name, leave any arguments behind */
    temp_command_name = my_strtok(buf, "!");

    temp_command = find_command(temp_command_name);
    if (temp_command == NULL) {
      logger(log_verification_error, basic)
        << "Error: Global host event handler command '"
        << temp_command_name << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the command for later */
    global_host_event_handler_ptr = temp_command;

    delete[] buf;
  }

  if (config.get_global_service_event_handler() != "") {

    /* check the event handler command */
    buf = my_strdup(qPrintable(config.get_global_service_event_handler()));

    /* get the command name, leave any arguments behind */
    temp_command_name = my_strtok(buf, "!");

    temp_command = find_command(temp_command_name);
    if (temp_command == NULL) {
      logger(log_verification_error, basic)
        << "Error: Global service event handler command '"
        << temp_command_name << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the command for later */
    global_service_event_handler_ptr = temp_command;

    delete[] buf;
  }

  /**************************************************/
  /* check obsessive processor commands...          */
  /**************************************************/
  if (verify_config == TRUE)
    printf("Checking obsessive compulsive processor commands...\n");

  if (!config.get_ocsp_command().isEmpty()) {

    buf = my_strdup(qPrintable(config.get_ocsp_command()));

    /* get the command name, leave any arguments behind */
    temp_command_name = my_strtok(buf, "!");

    temp_command = find_command(temp_command_name);
    if (temp_command == NULL) {
      logger(log_verification_error, basic)
        << "Error: Obsessive compulsive service processor command '"
        << temp_command_name << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the command for later */
    ocsp_command_ptr = temp_command;

    delete[] buf;
  }

  if (!config.get_ochp_command().isEmpty()) {

    buf = my_strdup(qPrintable(config.get_ochp_command()));

    /* get the command name, leave any arguments behind */
    temp_command_name = my_strtok(buf, "!");

    temp_command = find_command(temp_command_name);
    if (temp_command == NULL) {
      logger(log_verification_error, basic)
        << "Error: Obsessive compulsive host processor command '"
        << temp_command_name << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the command for later */
    ochp_command_ptr = temp_command;

    delete[] buf;
  }

  /* count number of services associated with each host (we need this for flap detection)... */
  for (temp_service = service_list;
       temp_service != NULL;
       temp_service = temp_service->next) {
    if ((temp_host = find_host(temp_service->host_name))) {
      temp_host->total_services++;
      temp_host->total_service_check_interval += static_cast<unsigned long>(temp_service->check_interval);
    }
  }

  if (verify_config == TRUE) {
    printf("\n");
    printf("Total Warnings: %d\n", warnings);
    printf("Total Errors:   %d\n", errors);
  }

  if (test_scheduling == TRUE)
    gettimeofday(&tv[3], NULL);

  if (test_scheduling == TRUE) {

    runtime[0] = (double)((double)(tv[1].tv_sec - tv[0].tv_sec)
			  + (double)((tv[1].tv_usec - tv[0].tv_usec) / 1000.0) / 1000.0);
    if (verify_circular_paths == TRUE)
      runtime[1] = (double)((double)(tv[2].tv_sec - tv[1].tv_sec)
			    + (double)((tv[2].tv_usec - tv[1].tv_usec) / 1000.0) / 1000.0);
    else
      runtime[1] = 0.0;
    runtime[2] = (double)((double)(tv[3].tv_sec - tv[2].tv_sec)
			  + (double)((tv[3].tv_usec - tv[2].tv_usec) / 1000.0) / 1000.0);
    runtime[3] = runtime[0] + runtime[1] + runtime[2];

    printf("Timing information on configuration verification is listed below.\n\n");

    printf("CONFIG VERIFICATION TIMES          (* = Potential for speedup with -x option)\n");
    printf("----------------------------------\n");
    printf("Object Relationships: %.6f sec\n", runtime[0]);
    printf("Circular Paths:       %.6f sec  *\n", runtime[1]);
    printf("Misc:                 %.6f sec\n", runtime[2]);
    printf("                      ============\n");
    printf("TOTAL:                %.6f sec  * = %.6f sec (%.1f%%) estimated savings\n",
	   runtime[3], runtime[1], (runtime[1] / runtime[3]) * 100.0);
    printf("\n\n");
  }

  return ((errors > 0) ? ERROR : OK);
}

/* do a pre-flight check to make sure object relationships make sense */
int pre_flight_object_check(int* w, int* e) {
  contact* temp_contact = NULL;
  contactgroup* temp_contactgroup = NULL;
  contactsmember* temp_contactsmember = NULL;
  contactgroupsmember* temp_contactgroupsmember = NULL;
  host* temp_host = NULL;
  host* temp_host2 = NULL;
  hostsmember* temp_hostsmember = NULL;
  hostgroup* temp_hostgroup = NULL;
  servicegroup* temp_servicegroup = NULL;
  servicesmember* temp_servicesmember = NULL;
  service* temp_service = NULL;
  service* temp_service2 = NULL;
  command* temp_command = NULL;
  timeperiod* temp_timeperiod = NULL;
  timeperiod* temp_timeperiod2 = NULL;
  timeperiodexclusion* temp_timeperiodexclusion = NULL;
  serviceescalation* temp_se = NULL;
  hostescalation* temp_he = NULL;
  servicedependency* temp_sd = NULL;
  hostdependency* temp_hd = NULL;
  int total_objects = 0;
  int warnings = 0;
  int errors = 0;

#ifdef TEST
  void* ptr = NULL;
  char* buf1 = "";
  char* buf2 = "";
  buf1 = "temptraxe1";
  buf2 = "Probe 2";
  for (temp_se = get_first_serviceescalation_by_service(buf1, buf2, &ptr);
       temp_se != NULL;
       temp_se = get_next_serviceescalation_by_service(buf1, buf2, &ptr)) {
    printf("FOUND ESCALATION FOR SVC '%s'/'%s': %d-%d/%.3f, PTR=%p\n", buf1,
           buf2, temp_se->first_notification, temp_se->last_notification,
           temp_se->notification_interval, ptr);
  }
  for (temp_he = get_first_hostescalation_by_host(buf1, &ptr);
       temp_he != NULL;
       temp_he = get_next_hostescalation_by_host(buf1, &ptr)) {
    printf("FOUND ESCALATION FOR HOST '%s': %d-%d/%d, PTR=%p\n", buf1,
           temp_he->first_notification, temp_he->last_notification,
           temp_he->notification_interval, ptr);
  }
#endif

  /*****************************************/
  /* check each service...                 */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking services...\n");
  // if (get_service_count() == 0) {
  //   logit(NSLOG_VERIFICATION_ERROR, TRUE, "Error: There are no services defined!");
  //   errors++;
  // }
  total_objects = 0;
  for (temp_service = service_list;
       temp_service != NULL;
       temp_service = temp_service->next) {
    total_objects++;
    check_service(temp_service, &warnings, &errors);
  }

  if (verify_config == TRUE)
    printf("\tChecked %d services.\n", total_objects);

  /*****************************************/
  /* check all hosts...                    */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking hosts...\n");

  // if (get_host_count() == 0) {
  //   logit(NSLOG_VERIFICATION_ERROR, TRUE, "Error: There are no hosts defined!");
  //   errors++;
  // }

  total_objects = 0;
  for (temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
    total_objects++;
    check_host(temp_host, &warnings, &errors);
  }

  if (verify_config == TRUE)
    printf("\tChecked %d hosts.\n", total_objects);

  /*****************************************/
  /* check each host group...              */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking host groups...\n");
  for (temp_hostgroup = hostgroup_list, total_objects = 0;
       temp_hostgroup != NULL;
       temp_hostgroup = temp_hostgroup->next, total_objects++) {

    /* check all group members */
    for (temp_hostsmember = temp_hostgroup->members;
	 temp_hostsmember != NULL;
         temp_hostsmember = temp_hostsmember->next) {

      temp_host = find_host(temp_hostsmember->host_name);
      if (temp_host == NULL) {
        logger(log_verification_error, basic)
          << "Error: Host '" << temp_hostsmember->host_name
          << "' specified in host group '" << temp_hostgroup->group_name
          << "' is not defined anywhere!";
        errors++;
      }

      /* save a pointer to this hostgroup for faster host/group membership lookups later */
      else
        add_object_to_objectlist(&temp_host->hostgroups_ptr, (void*)temp_hostgroup);

      /* save host pointer for later */
      temp_hostsmember->host_ptr = temp_host;
    }

    /* check for illegal characters in hostgroup name */
    if (use_precached_objects == FALSE) {
      if (contains_illegal_object_chars(temp_hostgroup->group_name) == TRUE) {
        logger(log_verification_error, basic)
          << "Error: The name of hostgroup '" << temp_hostgroup->group_name
          << "' contains one or more illegal characters.";
        errors++;
      }
    }
  }

  if (verify_config == TRUE)
    printf("\tChecked %d host groups.\n", total_objects);

  /*****************************************/
  /* check each service group...           */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking service groups...\n");
  for (temp_servicegroup = servicegroup_list, total_objects = 0;
       temp_servicegroup != NULL;
       temp_servicegroup = temp_servicegroup->next, total_objects++) {

    /* check all group members */
    for (temp_servicesmember = temp_servicegroup->members;
         temp_servicesmember != NULL;
         temp_servicesmember = temp_servicesmember->next) {

      temp_service = find_service(temp_servicesmember->host_name,
				  temp_servicesmember->service_description);
      if (temp_service == NULL) {
        logger(log_verification_error, basic)
          << "Error: Service '" << temp_servicesmember->service_description
          << "' on host '" << temp_servicesmember->host_name
          << "' specified in service group '" << temp_servicegroup->group_name
          << "' is not defined anywhere!";
        errors++;
      }

      /* save a pointer to this servicegroup for faster service/group membership lookups later */
      else
        add_object_to_objectlist(&temp_service->servicegroups_ptr, (void*)temp_servicegroup);

      /* save service pointer for later */
      temp_servicesmember->service_ptr = temp_service;
    }

    /* check for illegal characters in servicegroup name */
    if (use_precached_objects == FALSE) {
      if (contains_illegal_object_chars(temp_servicegroup->group_name) == TRUE) {
        logger(log_verification_error, basic)
          << "Error: The name of servicegroup '" << temp_servicegroup->group_name
          << "' contains one or more illegal characters.";
        errors++;
      }
    }
  }

  if (verify_config == TRUE)
    printf("\tChecked %d service groups.\n", total_objects);

  /*****************************************/
  /* check all contacts...                 */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking contacts...\n");
  // if (contact_list == NULL) {
  //   logit(NSLOG_VERIFICATION_ERROR, TRUE, "Error: There are no contacts defined!");
  //   errors++;
  // }
  for (temp_contact = contact_list, total_objects = 0;
       temp_contact != NULL;
       temp_contact = temp_contact->next, total_objects++) {
    check_contact(temp_contact, &warnings, &errors);
  }

  if (verify_config == TRUE)
    printf("\tChecked %d contacts.\n", total_objects);

  /*****************************************/
  /* check each contact group...           */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking contact groups...\n");
  for (temp_contactgroup = contactgroup_list, total_objects = 0;
       temp_contactgroup != NULL;
       temp_contactgroup = temp_contactgroup->next, total_objects++) {

    /* check all the group members */
    for (temp_contactsmember = temp_contactgroup->members;
         temp_contactsmember != NULL;
         temp_contactsmember = temp_contactsmember->next) {

      temp_contact = find_contact(temp_contactsmember->contact_name);
      if (temp_contact == NULL) {
        logger(log_verification_error, basic)
          << "Error: Contact '" << temp_contactsmember->contact_name
          << "' specified in contact group '" << temp_contactgroup->group_name
          << "' is not defined anywhere!";
        errors++;
      }

      /* save a pointer to this contactgroup for faster contact/group membership lookups later */
      else
        add_object_to_objectlist(&temp_contact->contactgroups_ptr, (void*)temp_contactgroup);

      /* save the contact pointer for later */
      temp_contactsmember->contact_ptr = temp_contact;
    }

    /* check for illegal characters in contactgroup name */
    if (use_precached_objects == FALSE) {
      if (contains_illegal_object_chars(temp_contactgroup->group_name) == TRUE) {
        logger(log_verification_error, basic)
          << "Error: The name of contact group '" << temp_contactgroup->group_name
          << "' contains one or more illegal characters.";
        errors++;
      }
    }
  }

  if (verify_config == TRUE)
    printf("\tChecked %d contact groups.\n", total_objects);

  /*****************************************/
  /* check all service escalations...     */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking service escalations...\n");

  for (temp_se = serviceescalation_list, total_objects = 0;
       temp_se != NULL;
       temp_se = temp_se->next, total_objects++) {

    /* find the service */
    temp_service = find_service(temp_se->host_name, temp_se->description);
    if (temp_service == NULL) {
      logger(log_verification_error, basic)
        << "Error: Service '" << temp_se->description
        << "' on host '" << temp_se->host_name
        << "' specified in service escalation is not defined anywhere!";
      errors++;
    }

    /* save the service pointer for later */
    temp_se->service_ptr = temp_service;

    /* find the timeperiod */
    if (temp_se->escalation_period != NULL) {
      temp_timeperiod = find_timeperiod(temp_se->escalation_period);
      if (temp_timeperiod == NULL) {
        logger(log_verification_error, basic)
          << "Error: Escalation period '" << temp_se->escalation_period
          << "' specified in service escalation for service '"
          << temp_se->description << "' on host '"
          << temp_se->host_name << "' is not defined anywhere!";
        errors++;
      }

      /* save the timeperiod pointer for later */
      temp_se->escalation_period_ptr = temp_timeperiod;
    }

    /* find the contacts */
    for (temp_contactsmember = temp_se->contacts;
	 temp_contactsmember != NULL;
         temp_contactsmember = temp_contactsmember->next) {

      /* find the contact */
      temp_contact = find_contact(temp_contactsmember->contact_name);
      if (temp_contact == NULL) {
        logger(log_verification_error, basic)
          << "Error: Contact '" << temp_contactsmember->contact_name
          << "' specified in service escalation for service '"
          << temp_se->description << "' on host '"
          << temp_se->host_name << "' is not defined anywhere!";
        errors++;
      }

      /* save the contact pointer for later */
      temp_contactsmember->contact_ptr = temp_contact;
    }

    /* check all contact groups */
    for (temp_contactgroupsmember = temp_se->contact_groups;
         temp_contactgroupsmember != NULL;
         temp_contactgroupsmember = temp_contactgroupsmember->next) {

      temp_contactgroup = find_contactgroup(temp_contactgroupsmember->group_name);

      if (temp_contactgroup == NULL) {
        logger(log_verification_error, basic)
          << "Error: Contact group '" << temp_contactgroupsmember->group_name
          << "' specified in service escalation for service '"
          << temp_se->description << "' on host '" << temp_se->host_name
          << "' is not defined anywhere!";
        errors++;
      }

      /* save the contact group pointer for later */
      temp_contactgroupsmember->group_ptr = temp_contactgroup;
    }
  }

  if (verify_config == TRUE)
    printf("\tChecked %d service escalations.\n", total_objects);

  /*****************************************/
  /* check all service dependencies...     */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking service dependencies...\n");

  for (temp_sd = servicedependency_list, total_objects = 0;
       temp_sd != NULL;
       temp_sd = temp_sd->next, total_objects++) {

    /* find the dependent service */
    temp_service = find_service(temp_sd->dependent_host_name,
				temp_sd->dependent_service_description);
    if (temp_service == NULL) {
      logger(log_verification_error, basic)
        << "Error: Dependent service '" << temp_sd->dependent_service_description
        << "' on host '" << temp_sd->dependent_host_name
        << "' specified in service dependency for service '"
        << temp_sd->service_description << "' on host '" << temp_sd->host_name
        << "' is not defined anywhere!";
      errors++;
    }

    /* save pointer for later */
    temp_sd->dependent_service_ptr = temp_service;

    /* find the service we're depending on */
    temp_service2 = find_service(temp_sd->host_name, temp_sd->service_description);
    if (temp_service2 == NULL) {
      logger(log_verification_error, basic)
        << "Error: Service '" << temp_sd->service_description << "' on host '"
        << temp_sd->host_name << "' specified in service dependency for service '"
        << temp_sd->dependent_service_description << "' on host '"
        << temp_sd->dependent_host_name << "' is not defined anywhere!";
      errors++;
    }

    /* save pointer for later */
    temp_sd->master_service_ptr = temp_service2;

    /* make sure they're not the same service */
    if (temp_service == temp_service2) {
      logger(log_verification_error, basic)
        << "Error: Service dependency definition for service '"
        << temp_sd->dependent_service_description << "' on host '"
        << temp_sd->dependent_host_name
        << "' is circular (it depends on itself)!";
      errors++;
    }

    /* find the timeperiod */
    if (temp_sd->dependency_period != NULL) {
      temp_timeperiod = find_timeperiod(temp_sd->dependency_period);
      if (temp_timeperiod == NULL) {
        logger(log_verification_error, basic)
          << "Error: Dependency period '" << temp_sd->dependency_period
          << "' specified in service dependency for service '"
          << temp_sd->dependent_service_description << "' on host '"
          << temp_sd->dependent_host_name << "' is not defined anywhere!";
        errors++;
      }

      /* save the timeperiod pointer for later */
      temp_sd->dependency_period_ptr = temp_timeperiod;
    }
  }

  if (verify_config == TRUE)
    printf("\tChecked %d service dependencies.\n", total_objects);

  /*****************************************/
  /* check all host escalations...     */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking host escalations...\n");

  for (temp_he = hostescalation_list, total_objects = 0;
       temp_he != NULL;
       temp_he = temp_he->next, total_objects++) {

    /* find the host */
    temp_host = find_host(temp_he->host_name);
    if (temp_host == NULL) {
      logger(log_verification_error, basic)
        << "Error: Host '" << temp_he->host_name
        << "' specified in host escalation is not defined anywhere!";
      errors++;
    }

    /* save the host pointer for later */
    temp_he->host_ptr = temp_host;

    /* find the timeperiod */
    if (temp_he->escalation_period != NULL) {
      temp_timeperiod = find_timeperiod(temp_he->escalation_period);
      if (temp_timeperiod == NULL) {
        logger(log_verification_error, basic)
          << "Error: Escalation period '" << temp_he->escalation_period
          << "' specified in host escalation for host '" << temp_he->host_name
          << "' is not defined anywhere!";
        errors++;
      }

      /* save the timeperiod pointer for later */
      temp_he->escalation_period_ptr = temp_timeperiod;
    }

    /* find the contacts */
    for (temp_contactsmember = temp_he->contacts;
	 temp_contactsmember != NULL;
         temp_contactsmember = temp_contactsmember->next) {

      /* find the contact */
      temp_contact = find_contact(temp_contactsmember->contact_name);
      if (temp_contact == NULL) {
        logger(log_verification_error, basic)
          << "Error: Contact '" << temp_contactsmember->contact_name
          << "' specified in host escalation for host '"
          << temp_he->host_name << "' is not defined anywhere!";
        errors++;
      }

      /* save the contact pointer for later */
      temp_contactsmember->contact_ptr = temp_contact;
    }

    /* check all contact groups */
    for (temp_contactgroupsmember = temp_he->contact_groups;
         temp_contactgroupsmember != NULL;
         temp_contactgroupsmember = temp_contactgroupsmember->next) {

      temp_contactgroup = find_contactgroup(temp_contactgroupsmember->group_name);

      if (temp_contactgroup == NULL) {
        logger(log_verification_error, basic)
          << "Error: Contact group '" << temp_contactgroupsmember->group_name
          << "' specified in host escalation for host '"
          << temp_he->host_name << "' is not defined anywhere!";
        errors++;
      }

      /* save the contact group pointer for later */
      temp_contactgroupsmember->group_ptr = temp_contactgroup;
    }
  }

  if (verify_config == TRUE)
    printf("\tChecked %d host escalations.\n", total_objects);

  /*****************************************/
  /* check all host dependencies...     */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking host dependencies...\n");

  for (temp_hd = hostdependency_list, total_objects = 0;
       temp_hd != NULL;
       temp_hd = temp_hd->next, total_objects++) {

    /* find the dependent host */
    temp_host = find_host(temp_hd->dependent_host_name);
    if (temp_host == NULL) {
      logger(log_verification_error, basic)
        << "Error: Dependent host specified in host dependency for host '"
        << temp_hd->dependent_host_name << "' is not defined anywhere!";
      errors++;
    }

    /* save pointer for later */
    temp_hd->dependent_host_ptr = temp_host;

    /* find the host we're depending on */
    temp_host2 = find_host(temp_hd->host_name);
    if (temp_host2 == NULL) {
      logger(log_verification_error, basic)
        << "Error: Host specified in host dependency for host '"
        << temp_hd->dependent_host_name << "' is not defined anywhere!";
      errors++;
    }

    /* save pointer for later */
    temp_hd->master_host_ptr = temp_host2;

    /* make sure they're not the same host */
    if (temp_host == temp_host2) {
      logger(log_verification_error, basic)
        << "Error: Host dependency definition for host '"
        <<  temp_hd->dependent_host_name
        << "' is circular (it depends on itself)!";
      errors++;
    }

    /* find the timeperiod */
    if (temp_hd->dependency_period != NULL) {
      temp_timeperiod = find_timeperiod(temp_hd->dependency_period);
      if (temp_timeperiod == NULL) {
        logger(log_verification_error, basic)
          << "Error: Dependency period '" << temp_hd->dependency_period
          << "' specified in host dependency for host '"
          << temp_hd->dependent_host_name << "' is not defined anywhere!";
        errors++;
      }

      /* save the timeperiod pointer for later */
      temp_hd->dependency_period_ptr = temp_timeperiod;
    }
  }

  if (verify_config == TRUE)
    printf("\tChecked %d host dependencies.\n", total_objects);

  /*****************************************/
  /* check all commands...                 */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking commands...\n");

  for (temp_command = command_list, total_objects = 0;
       temp_command != NULL;
       temp_command = temp_command->next, total_objects++) {

    /* check for illegal characters in command name */
    if (use_precached_objects == FALSE) {
      if (contains_illegal_object_chars(temp_command->name) == TRUE) {
        logger(log_verification_error, basic)
          << "Error: The name of command '" << temp_command->name
          << "' contains one or more illegal characters.";
        errors++;
      }
    }
  }

  if (verify_config == TRUE)
    printf("\tChecked %d commands.\n", total_objects);

  /*****************************************/
  /* check all timeperiods...              */
  /*****************************************/
  if (verify_config == TRUE)
    printf("Checking time periods...\n");

  for (temp_timeperiod = timeperiod_list, total_objects = 0;
       temp_timeperiod != NULL;
       temp_timeperiod = temp_timeperiod->next, total_objects++) {

    /* check for illegal characters in timeperiod name */
    if (use_precached_objects == FALSE) {
      if (contains_illegal_object_chars(temp_timeperiod->name) == TRUE) {
        logger(log_verification_error, basic)
          << "Error: The name of time period '" << temp_timeperiod->name
          << "' contains one or more illegal characters.";
        errors++;
      }
    }

    /* check for valid timeperiod names in exclusion list */
    for (temp_timeperiodexclusion = temp_timeperiod->exclusions;
         temp_timeperiodexclusion != NULL;
         temp_timeperiodexclusion = temp_timeperiodexclusion->next) {

      temp_timeperiod2 = find_timeperiod(temp_timeperiodexclusion->timeperiod_name);
      if (temp_timeperiod2 == NULL) {
        logger(log_verification_error, basic)
          << "Error: Excluded time period '"
          << temp_timeperiodexclusion->timeperiod_name
          << "' specified in timeperiod '" << temp_timeperiod->name
          << "' is not defined anywhere!";
        errors++;
      }

      /* save the timeperiod pointer for later */
      temp_timeperiodexclusion->timeperiod_ptr = temp_timeperiod2;
    }
  }

  if (verify_config == TRUE)
    printf("\tChecked %d time periods.\n", total_objects);

  /* update warning and error count */
  *w += warnings;
  *e += errors;

  return ((errors > 0) ? ERROR : OK);
}

/* dfs status values */
#define DFS_UNCHECKED                    0      /* default value */
#define DFS_TEMP_CHECKED                 1      /* check just one time */
#define DFS_OK                           2      /* no problem */
#define DFS_NEAR_LOOP                    3      /* has trouble sons */
#define DFS_LOOPY                        4      /* is a part of a loop */

#define dfs_get_status(h) h->circular_path_checked
#define dfs_unset_status(h) h->circular_path_checked = 0
#define dfs_set_status(h, flag) h->circular_path_checked = (flag)
#define dfs_host_status(h) (h ? dfs_get_status(h) : DFS_OK)

/**
 * Modified version of Depth-first Search
 * http://en.wikipedia.org/wiki/Depth-first_search
 */
static int dfs_host_path(host* root) {
  hostsmember* child = NULL;

  if (!root)
    return (DFS_NEAR_LOOP);

  if (dfs_get_status(root) != DFS_UNCHECKED)
    return (dfs_get_status(root));

  /* Mark the root temporary checked */
  dfs_set_status(root, DFS_TEMP_CHECKED);

  /* We are scanning the children */
  for (child = root->child_hosts; child != NULL; child = child->next) {
    int child_status = dfs_get_status(child->host_ptr);

    /* If a child is not checked, check it */
    if (child_status == DFS_UNCHECKED)
      child_status = dfs_host_path(child->host_ptr);

    /* If a child already temporary checked, its a problem,
     * loop inside, and its a acked status */
    if (child_status == DFS_TEMP_CHECKED) {
      dfs_set_status(child->host_ptr, DFS_LOOPY);
      dfs_set_status(root, DFS_LOOPY);
    }

    /* If a child already temporary checked, its a problem, loop inside */
    if (child_status == DFS_NEAR_LOOP || child_status == DFS_LOOPY) {
      /* if a node is know to be part of a loop, do not let it be less */
      if (dfs_get_status(root) != DFS_LOOPY)
        dfs_set_status(root, DFS_NEAR_LOOP);

      /* we already saw this child, it's a problem */
      dfs_set_status(child->host_ptr, DFS_LOOPY);
    }
  }

  /*
   * If root have been modified, do not set it OK
   * A node is OK if and only if all of his children are OK
   * If it does not have child, goes ok
   */
  if (dfs_get_status(root) == DFS_TEMP_CHECKED)
    dfs_set_status(root, DFS_OK);
  return (dfs_get_status(root));
}

/* check for circular paths and dependencies */
int pre_flight_circular_check(int* w, int* e) {
  host* temp_host = NULL;
  servicedependency* temp_sd = NULL;
  servicedependency* temp_sd2 = NULL;
  hostdependency* temp_hd = NULL;
  hostdependency* temp_hd2 = NULL;
  int found = FALSE;
  int warnings = 0;
  int errors = 0;

  /* bail out if we aren't supposed to verify circular paths */
  if (verify_circular_paths == FALSE)
    return (OK);

  /********************************************/
  /* check for circular paths between hosts   */
  /********************************************/
  if (verify_config == TRUE)
    printf("Checking for circular paths between hosts...\n");

  /* check routes between all hosts */
  found = FALSE;

  /* We clean the dsf status from previous check */
  for (temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
    dfs_set_status(temp_host, DFS_UNCHECKED);
  }

  for (temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
    if (dfs_host_path(temp_host) == DFS_LOOPY)
      errors = 1;
  }

  for (temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {
    if (dfs_get_status(temp_host) == DFS_LOOPY)
      logger(log_verification_error, basic)
        << "Error: The host '" << temp_host->name
        << "' is part of a circular parent/child chain!";
    /* clean DFS status */
    dfs_set_status(temp_host, DFS_UNCHECKED);
  }

  /********************************************/
  /* check for circular dependencies         */
  /********************************************/
  if (verify_config == TRUE)
    printf("Checking for circular host and service dependencies...\n");

  /* check execution dependencies between all services */
  for (temp_sd = servicedependency_list;
       temp_sd != NULL;
       temp_sd = temp_sd->next) {

    /* clear checked flag for all dependencies */
    for (temp_sd2 = servicedependency_list;
	 temp_sd2 != NULL;
         temp_sd2 = temp_sd2->next)
      temp_sd2->circular_path_checked = FALSE;

    found = check_for_circular_servicedependency_path(temp_sd, temp_sd, EXECUTION_DEPENDENCY);
    if (found == TRUE) {
      logger(log_verification_error, basic)
        << "Error: A circular execution dependency (which could result in a " \
        "deadlock) exists for service '" << temp_sd->service_description
        << "' on host '" << temp_sd->host_name << "'!";
      errors++;
    }
  }

  /* check notification dependencies between all services */
  for (temp_sd = servicedependency_list;
       temp_sd != NULL;
       temp_sd = temp_sd->next) {

    /* clear checked flag for all dependencies */
    for (temp_sd2 = servicedependency_list;
	 temp_sd2 != NULL;
         temp_sd2 = temp_sd2->next)
      temp_sd2->circular_path_checked = FALSE;

    found = check_for_circular_servicedependency_path(temp_sd, temp_sd, NOTIFICATION_DEPENDENCY);
    if (found == TRUE) {
      logger(log_verification_error, basic)
        << "Error: A circular notification dependency (which could result in a " \
        "deadlock) exists for service '" << temp_sd->service_description
        << "' on host '" << temp_sd->host_name << "'!";
      errors++;
    }
  }

  /* check execution dependencies between all hosts */
  for (temp_hd = hostdependency_list; temp_hd != NULL; temp_hd = temp_hd->next) {

    /* clear checked flag for all dependencies */
    for (temp_hd2 = hostdependency_list;
	 temp_hd2 != NULL;
         temp_hd2 = temp_hd2->next)
      temp_hd2->circular_path_checked = FALSE;

    found = check_for_circular_hostdependency_path(temp_hd, temp_hd, EXECUTION_DEPENDENCY);
    if (found == TRUE) {
      logger(log_verification_error, basic)
        << "Error: A circular execution dependency (which could result in a " \
        "deadlock) exists for host '" << temp_hd->host_name << "'!";
      errors++;
    }
  }

  /* check notification dependencies between all hosts */
  for (temp_hd = hostdependency_list; temp_hd != NULL; temp_hd = temp_hd->next) {

    /* clear checked flag for all dependencies */
    for (temp_hd2 = hostdependency_list;
	 temp_hd2 != NULL;
         temp_hd2 = temp_hd2->next)
      temp_hd2->circular_path_checked = FALSE;

    found = check_for_circular_hostdependency_path(temp_hd, temp_hd, NOTIFICATION_DEPENDENCY);
    if (found == TRUE) {
      logger(log_verification_error, basic)
        << "Error: A circular notification dependency (which could result in a " \
        "deadlock) exists for host '" << temp_hd->host_name << "'!";
      errors++;
    }
  }

  /* update warning and error count */
  if (w != NULL)
    *w += warnings;
  if (e != NULL)
    *e += errors;

  return ((errors > 0) ? ERROR : OK);
}

int check_service(service* svc, int* w, int* e) {
  int errors = 0;
  int warnings = 0;

  /* check for a valid host */
  host* temp_host = find_host(svc->host_name);

  /* we couldn't find an associated host! */

  if (!temp_host) {
    logger(log_verification_error, basic)
      << "Error: Host '" << svc->host_name << "' specified in service " \
      "'" << svc->description << "' not defined anywhere!";
    errors++;
  }

  /* save the host pointer for later */
  svc->host_ptr = temp_host;

  /* add a reverse link from the host to the service for faster lookups later */
  add_service_link_to_host(temp_host, svc);

  /* check the event handler command */
  if (svc->event_handler != NULL) {

    /* check the event handler command */
    char* buf = my_strdup(svc->event_handler);

    /* get the command name, leave any arguments behind */
    char* temp_command_name = my_strtok(buf, "!");

    command* temp_command = find_command(temp_command_name);
    if (temp_command == NULL) {
      logger(log_verification_error, basic)
        << "Error: Event handler command '" << temp_command_name
        << "' specified in service '" << svc->description
        << "' for host '" << svc->host_name << "' not defined anywhere";
      errors++;
    }

    delete[] buf;

    /* save the pointer to the event handler for later */
    svc->event_handler_ptr = temp_command;
  }

  /* check the service check_command */
  char* buf = my_strdup(svc->service_check_command);

  /* get the command name, leave any arguments behind */
  char* temp_command_name = my_strtok(buf, "!");

  command* temp_command = find_command(temp_command_name);
  if (temp_command == NULL) {
    logger(log_verification_error, basic)
      << "Error: Service check command '" << temp_command_name
      << "' specified in service '" << svc->description
      << "' for host '" << svc->host_name << "' not defined anywhere!";
    errors++;
  }

  delete[] buf;

  /* save the pointer to the check command for later */
  svc->check_command_ptr = temp_command;

  /* check for sane recovery options */
  if (svc->notify_on_recovery == TRUE
      && svc->notify_on_warning == FALSE
      && svc->notify_on_critical == FALSE) {
    logger(log_verification_error, basic)
      << "Warning: Recovery notification option in service '"
      << svc->description << "' for host '" << svc->host_name
      << "' doesn't make any sense - specify warning and/or critical " \
      "options as well";
    warnings++;
  }

  /* check for valid contacts */
  for (contactsmember* temp_contactsmember = svc->contacts;
       temp_contactsmember != NULL;
       temp_contactsmember = temp_contactsmember->next) {

    contact* temp_contact = find_contact(temp_contactsmember->contact_name);

    if (temp_contact == NULL) {
      logger(log_verification_error, basic)
        << "Error: Contact '" << temp_contactsmember->contact_name
        << "' specified in service '" << svc->description << "' for " \
        "host '" << svc->host_name << "' is not defined anywhere!";
      errors++;
    }

    /* save the contact pointer for later */
    temp_contactsmember->contact_ptr = temp_contact;
  }

  /* check all contact groupss */
  for (contactgroupsmember* temp_contactgroupsmember = svc->contact_groups;
       temp_contactgroupsmember != NULL;
       temp_contactgroupsmember = temp_contactgroupsmember->next) {

    contactgroup* temp_contactgroup = find_contactgroup(temp_contactgroupsmember->group_name);

    if (temp_contactgroup == NULL) {
      logger(log_verification_error, basic)
        << "Error: Contact group '" << temp_contactgroupsmember->group_name
        << "' specified in service '" << svc->description << "' for " \
        "host '" << svc->host_name << "' is not defined anywhere!";
      errors++;
    }

    /* save the contact group pointer for later */
    temp_contactgroupsmember->group_ptr = temp_contactgroup;
  }

  /* check to see if there is at least one contact/group */
  if (svc->contacts == NULL && svc->contact_groups == NULL) {
    logger(log_verification_error, basic)
      << "Warning: Service '" << svc->description << "' on host '"
      << svc->host_name << "' has no default contacts or " \
      "contactgroups defined!";
    warnings++;
  }

  /* verify service check timeperiod */
  if (svc->check_period == NULL) {
    logger(log_verification_error, basic)
      << "Warning: Service '" << svc->description << "' on host '"
      << svc->host_name << "' has no check time period defined!";
    warnings++;
  }
  else {
    timeperiod* temp_timeperiod = find_timeperiod(svc->check_period);
    if (temp_timeperiod == NULL) {
      logger(log_verification_error, basic)
        << "Error: Check period '" << svc->check_period
        << "' specified for service '" << svc->description
        << "' on host '" << svc->host_name << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the check timeperiod for later */
    svc->check_period_ptr = temp_timeperiod;
  }

  /* check service notification timeperiod */
  if (svc->notification_period == NULL) {
    logger(log_verification_error, basic)
      << "Warning: Service '" << svc->description << "' on host " \
      "'" << svc->host_name << "' has no notification time period defined!";
    warnings++;
  }

  else {
    timeperiod* temp_timeperiod = find_timeperiod(svc->notification_period);
    if (temp_timeperiod == NULL) {
      logger(log_verification_error, basic)
        << "Error: Notification period '" << svc->notification_period
        << "' specified for service '" << svc->description << "' on " \
        "host '" << svc->host_name << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the notification timeperiod for later */
    svc->notification_period_ptr = temp_timeperiod;
  }

  /* see if the notification interval is less than the check interval */
  if (svc->notification_interval < svc->check_interval
      && svc->notification_interval != 0) {
    logger(log_verification_error, basic)
      << "Warning: Service '" << svc->description << "' on host '"
      << svc->host_name << "'  has a notification interval less than " \
      "its check interval!  Notifications are only re-sent after " \
      "checks are made, so the effective notification interval will " \
      "be that of the check interval.";
    warnings++;
  }

  /* check for illegal characters in service description */
  if (use_precached_objects == FALSE) {
    if (contains_illegal_object_chars(svc->description) == TRUE) {
      logger(log_verification_error, basic)
        << "Error: The description string for service '"
        << svc->description << "' on host '" << svc->host_name
        << "' contains one or more illegal characters.";
      errors++;
    }
  }

  if (w != NULL)
    *w += warnings;
  if (e != NULL)
    *e += errors;
  return (errors == 0);
}

int check_host(host* hst, int* w, int* e) {
  int warnings = 0;
  int errors = 0;

  /* make sure each host has at least one service associated with it */
  /* 02/21/08 NOTE: this is extremely inefficient */
  if (use_precached_objects == FALSE
      && config.get_use_large_installation_tweaks() == false) {

    bool found = false;
    for (service* temp_service = service_list;
	 temp_service != NULL;
	 temp_service = temp_service->next) {
      if (!strcmp(temp_service->host_name, hst->name)) {
	found = true;
	break;
      }
    }

    /* we couldn't find a service associated with this host! */
    if (found == false) {
      logger(log_verification_error, basic)
        << "Warning: Host '" << hst->name
        << "' has no services associated with it!";
      warnings++;
    }
  }

  /* check the event handler command */
  if (hst->event_handler != NULL) {

    /* check the event handler command */
    char* buf = my_strdup(hst->event_handler);

    /* get the command name, leave any arguments behind */
    char* temp_command_name = my_strtok(buf, "!");

    command* temp_command = find_command(temp_command_name);
    if (temp_command == NULL) {
      logger(log_verification_error, basic)
        << "Error: Event handler command '" << temp_command_name
        << "' specified for host '" << hst->name
        << "' not defined anywhere";
      errors++;
    }

    delete[] buf;

    /* save the pointer to the event handler command for later */
    hst->event_handler_ptr = temp_command;
  }

  /* hosts that don't have check commands defined shouldn't ever be checked... */
  if (hst->host_check_command != NULL) {

    /* check the host check_command */
    char* buf = my_strdup(hst->host_check_command);

    /* get the command name, leave any arguments behind */
    char* temp_command_name = my_strtok(buf, "!");

    command* temp_command = find_command(temp_command_name);
    if (temp_command == NULL) {
      logger(log_verification_error, basic)
        << "Error: Host check command '" << temp_command_name
        << "' specified for host '" << hst->name
        << "' is not defined anywhere!",
      errors++;
    }

    /* save the pointer to the check command for later */
    hst->check_command_ptr = temp_command;

    delete[] buf;
  }

  /* check host check timeperiod */
  if (hst->check_period != NULL) {
    timeperiod* temp_timeperiod = find_timeperiod(hst->check_period);
    if (temp_timeperiod == NULL) {
      logger(log_verification_error, basic)
        << "Error: Check period '" << hst->check_period
        << "' specified for host '" << hst->name
        << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the check timeperiod for later */
    hst->check_period_ptr = temp_timeperiod;
  }

  /* check all contacts */
  for (contactsmember* temp_contactsmember = hst->contacts;
       temp_contactsmember != NULL;
       temp_contactsmember = temp_contactsmember->next) {

    contact* temp_contact = find_contact(temp_contactsmember->contact_name);

    if (temp_contact == NULL) {
      logger(log_verification_error, basic)
        << "Error: Contact '" << temp_contactsmember->contact_name
        << "' specified in host '" << hst->name
        << "' is not defined anywhere!";
      errors++;
    }

    /* save the contact pointer for later */
    temp_contactsmember->contact_ptr = temp_contact;
  }

  /* check all contact groups */
  for (contactgroupsmember* temp_contactgroupsmember = hst->contact_groups;
       temp_contactgroupsmember != NULL;
       temp_contactgroupsmember = temp_contactgroupsmember->next) {

    contactgroup* temp_contactgroup = find_contactgroup(temp_contactgroupsmember->group_name);

    if (temp_contactgroup == NULL) {
      logger(log_verification_error, basic)
        << "Error: Contact group '" << temp_contactgroupsmember->group_name
        << "' specified in host '" << hst->name << "' is not defined anywhere!";
      errors++;
    }

    /* save the contact group pointer for later */
    temp_contactgroupsmember->group_ptr = temp_contactgroup;
  }

  /* check to see if there is at least one contact/group */
  if (hst->contacts == NULL && hst->contact_groups == NULL) {
    logger(log_verification_error, basic)
      << "Warning: Host '" << hst->name << "' has no default contacts " \
      "or contactgroups defined!";
    warnings++;
  }

  /* check notification timeperiod */
  if (hst->notification_period != NULL) {
    timeperiod* temp_timeperiod = find_timeperiod(hst->notification_period);
    if (temp_timeperiod == NULL) {
      logger(log_verification_error, basic)
        << "Error: Notification period '" << hst->notification_period
        << "' specified for host '" << hst->name << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the notification timeperiod for later */
    hst->notification_period_ptr = temp_timeperiod;
  }

  /* check all parent parent host */
  for (hostsmember* temp_hostsmember = hst->parent_hosts;
       temp_hostsmember != NULL;
       temp_hostsmember = temp_hostsmember->next) {

    host* hst2 = NULL;
    if ((hst2 = find_host(temp_hostsmember->host_name)) == NULL) {
      logger(log_verification_error, basic)
        << "Error: '" << temp_hostsmember->host_name << "' is not a " \
        "valid parent for host '" << hst->name << "'!";
      errors++;
    }

    /* save the parent host pointer for later */
    temp_hostsmember->host_ptr = hst2;

    /* add a reverse (child) link to make searches faster later on */
    add_child_link_to_host(hst2, hst);
  }

  /* check for sane recovery options */
  if (hst->notify_on_recovery == TRUE
      && hst->notify_on_down == FALSE
      && hst->notify_on_unreachable == FALSE) {
    logger(log_verification_error, basic)
      << "Warning: Recovery notification option in host '" << hst->name
      << "' definition doesn't make any sense - specify down and/or " \
      "unreachable options as well";
    warnings++;
  }

  /* check for illegal characters in host name */
  if (use_precached_objects == FALSE) {
    if (contains_illegal_object_chars(hst->name) == TRUE) {
      logger(log_verification_error, basic)
        << "Error: The name of host '" << hst->name
        << "' contains one or more illegal characters.";
      errors++;
    }
  }

  if (w != NULL)
    *w += warnings;
  if (e != NULL)
    *e += errors;
  return (errors == 0);
}

int check_contact(contact* cntct, int* w, int* e) {
  int warnings = 0;
  int errors = 0;

  /* check service notification commands */
  if (cntct->service_notification_commands == NULL) {
    logger(log_verification_error, basic)
      << "Error: Contact '" << cntct->name << "' has no service " \
      "notification commands defined!";
    errors++;
  }
  else
    for (commandsmember* temp_commandsmember = cntct->service_notification_commands;
	 temp_commandsmember != NULL;
	 temp_commandsmember = temp_commandsmember->next) {

      /* check the host notification command */
      char* buf = my_strdup(temp_commandsmember->cmd);

      /* get the command name, leave any arguments behind */
      char* temp_command_name = my_strtok(buf, "!");

      command* temp_command = find_command(temp_command_name);
      if (temp_command == NULL) {
        logger(log_verification_error, basic)
          << "Error: Service notification command '"
          << temp_command_name << "' specified for contact '"
          << cntct->name << "' is not defined anywhere!";
	errors++;
      }

      /* save pointer to the command for later */
      temp_commandsmember->command_ptr = temp_command;

      delete[] buf;
    }

  /* check host notification commands */
  if (cntct->host_notification_commands == NULL) {
    logger(log_verification_error, basic)
      << "Error: Contact '" << cntct->name << "' has no host " \
      "notification commands defined!";
    errors++;
  }
  else
    for (commandsmember* temp_commandsmember = cntct->host_notification_commands;
	 temp_commandsmember != NULL;
	 temp_commandsmember = temp_commandsmember->next) {

      /* check the host notification command */
      char* buf = my_strdup(temp_commandsmember->cmd);

      /* get the command name, leave any arguments behind */
      char* temp_command_name = my_strtok(buf, "!");

      command* temp_command = find_command(temp_command_name);
      if (temp_command == NULL) {
        logger(log_verification_error, basic)
          << "Error: Host notification command '" << temp_command_name
          << "' specified for contact '" << cntct->name
          << "' is not defined anywhere!";
	errors++;
      }

      /* save pointer to the command for later */
      temp_commandsmember->command_ptr = temp_command;

      delete[] buf;
    }

  /* check service notification timeperiod */
  if (cntct->service_notification_period == NULL) {
    logger(log_verification_error, basic)
      << "Warning: Contact '" << cntct->name << "' has no service " \
      "notification time period defined!";
    warnings++;
  }

  else {
    timeperiod* temp_timeperiod = find_timeperiod(cntct->service_notification_period);
    if (temp_timeperiod == NULL) {
      logger(log_verification_error, basic)
        << "Error: Service notification period '"
        << cntct->service_notification_period << "' specified for contact '"
        << cntct->name << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the service notification timeperiod for later */
    cntct->service_notification_period_ptr = temp_timeperiod;
  }

  /* check host notification timeperiod */
  if (cntct->host_notification_period == NULL) {
    logger(log_verification_error, basic)
      << "Warning: Contact '" << cntct->name << "' has no host " \
      "notification time period defined!";
    warnings++;
  }

  else {
    timeperiod* temp_timeperiod = find_timeperiod(cntct->host_notification_period);
    if (temp_timeperiod == NULL) {
      logger(log_verification_error, basic)
        << "Error: Host notification period '" << cntct->host_notification_period
        << "' specified for contact '" << cntct->name << "' is not defined anywhere!";
      errors++;
    }

    /* save the pointer to the host notification timeperiod for later */
    cntct->host_notification_period_ptr = temp_timeperiod;
  }

  /* check for sane host recovery options */
  if (cntct->notify_on_host_recovery == TRUE
      && cntct->notify_on_host_down == FALSE
      && cntct->notify_on_host_unreachable == FALSE) {
    logger(log_verification_error, basic)
      << "Warning: Host recovery notification option for contact '"
      << cntct->name << "' doesn't make any sense - specify down " \
      "and/or unreachable options as well";
    warnings++;
  }

  /* check for sane service recovery options */
  if (cntct->notify_on_service_recovery == TRUE
      && cntct->notify_on_service_critical == FALSE
      && cntct->notify_on_service_warning == FALSE) {
    logger(log_verification_error, basic)
      << "Warning: Service recovery notification option for contact '"
      << cntct->name << "' doesn't make any sense - specify critical " \
      "and/or warning options as well";
    warnings++;
  }

  /* check for illegal characters in contact name */
  if (use_precached_objects == FALSE) {
    if (contains_illegal_object_chars(cntct->name) == TRUE) {
      logger(log_verification_error, basic)
        << "Error: The name of contact '" << cntct->name
        << "' contains one or more illegal characters.";
      errors++;
    }
  }

  if (w != NULL)
    *w += warnings;
  if (e != NULL)
    *e += errors;
  return (errors == 0);
}
