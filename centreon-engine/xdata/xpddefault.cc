/*
** Copyright 2000-2008 Ethan Galstad
** Copyright 2011      Merethis
**
** This file is part of Centreon Scheduler.
**
** Centreon Scheduler is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Scheduler is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Scheduler. If not, see
** <http://www.gnu.org/licenses/>.
*/

/*********** COMMON HEADER FILES ***********/

#include "conf.hh"
#include "common.hh"
#include "objects.hh"
#include "macros.hh"
#include "utils.hh"
#include "events.hh"
#include "logging.hh"

/**** DATA INPUT-SPECIFIC HEADER FILES ****/

#include "xpddefault.hh"

static int             xpddefault_perfdata_timeout;

static char*           xpddefault_host_perfdata_command = NULL;
static char*           xpddefault_service_perfdata_command = NULL;
static command*        xpddefault_host_perfdata_command_ptr = NULL;
static command*        xpddefault_service_perfdata_command_ptr = NULL;

static char*           xpddefault_host_perfdata_file_template = NULL;
static char*           xpddefault_service_perfdata_file_template = NULL;

static char*           xpddefault_host_perfdata_file = NULL;
static char*           xpddefault_service_perfdata_file = NULL;

static int             xpddefault_host_perfdata_file_append = TRUE;
static int             xpddefault_service_perfdata_file_append = TRUE;
static int             xpddefault_host_perfdata_file_pipe = FALSE;
static int             xpddefault_service_perfdata_file_pipe = FALSE;

static unsigned long   xpddefault_host_perfdata_file_processing_interval = 0L;
static unsigned long   xpddefault_service_perfdata_file_processing_interval = 0L;

static char*           xpddefault_host_perfdata_file_processing_command = NULL;
static char*           xpddefault_service_perfdata_file_processing_command = NULL;
static command*        xpddefault_host_perfdata_file_processing_command_ptr = NULL;
static command*        xpddefault_service_perfdata_file_processing_command_ptr = NULL;

static FILE*           xpddefault_host_perfdata_fp = NULL;
static FILE*           xpddefault_service_perfdata_fp = NULL;
static int             xpddefault_host_perfdata_fd = -1;
static int             xpddefault_service_perfdata_fd = -1;

static pthread_mutex_t xpddefault_host_perfdata_fp_lock;
static pthread_mutex_t xpddefault_service_perfdata_fp_lock;

/******************************************************************/
/***************** COMMON CONFIG INITIALIZATION  ******************/
/******************************************************************/

/* grabs configuration information from main config file */
int xpddefault_grab_config_info(char* config_file) {
  char* input = NULL;
  mmapfile* thefile = NULL;

  /* open the config file for reading */
  if ((thefile = mmap_fopen(config_file)) == NULL) {
    logit(NSLOG_CONFIG_ERROR, TRUE,
          "Error: Could not open main config file '%s' for reading performance variables!\n",
          config_file);
    return (ERROR);
  }

  /* read in all lines from the config file */
  while (1) {
    /* free memory */
    delete[] input;

    /* read the next line */
    if ((input = mmap_fgets_multiline(thefile)) == NULL)
      break;

    /* skip blank lines and comments */
    if (input[0] == '#' || input[0] == '\x0')
      continue;

    strip(input);
    xpddefault_grab_config_directives(input);
  }

  /* free memory and close the file */
  delete[] input;
  mmap_fclose(thefile);

  return (OK);
}

/* processes a single directive */
int xpddefault_grab_config_directives(char* input) {
  char* temp_ptr = NULL;
  char* varname = NULL;
  char* varvalue = NULL;

  /* get the variable name */
  if ((temp_ptr = my_strtok(input, "=")) == NULL)
    return (ERROR);
  varname = my_strdup(temp_ptr);

  /* get the variable value */
  if ((temp_ptr = my_strtok(NULL, "\n")) == NULL) {
    delete[] varname;
    return (ERROR);
  }
  varvalue = my_strdup(temp_ptr);

  if (!strcmp(varname, "perfdata_timeout")) {
    strip(varvalue);
    xpddefault_perfdata_timeout = atoi(varvalue);
  }
  else if (!strcmp(varname, "host_perfdata_command"))
    xpddefault_host_perfdata_command = my_strdup(varvalue);
  else if (!strcmp(varname, "service_perfdata_command"))
    xpddefault_service_perfdata_command = my_strdup(varvalue);
  else if (!strcmp(varname, "host_perfdata_file_template"))
    xpddefault_host_perfdata_file_template = my_strdup(varvalue);
  else if (!strcmp(varname, "service_perfdata_file_template"))
    xpddefault_service_perfdata_file_template = my_strdup(varvalue);
  else if (!strcmp(varname, "host_perfdata_file"))
    xpddefault_host_perfdata_file = my_strdup(varvalue);
  else if (!strcmp(varname, "service_perfdata_file"))
    xpddefault_service_perfdata_file = my_strdup(varvalue);
  else if (!strcmp(varname, "host_perfdata_file_mode")) {
    xpddefault_host_perfdata_file_pipe = FALSE;
    if (strstr(varvalue, "p") != NULL)
      xpddefault_host_perfdata_file_pipe = TRUE;
    else if (strstr(varvalue, "w") != NULL)
      xpddefault_host_perfdata_file_append = FALSE;
    else
      xpddefault_host_perfdata_file_append = TRUE;
  }
  else if (!strcmp(varname, "service_perfdata_file_mode")) {
    xpddefault_service_perfdata_file_pipe = FALSE;
    if (strstr(varvalue, "p") != NULL)
      xpddefault_service_perfdata_file_pipe = TRUE;
    else if (strstr(varvalue, "w") != NULL)
      xpddefault_service_perfdata_file_append = FALSE;
    else
      xpddefault_service_perfdata_file_append = TRUE;
  }
  else if (!strcmp(varname, "host_perfdata_file_processing_interval"))
    xpddefault_host_perfdata_file_processing_interval = strtoul(varvalue, NULL, 0);
  else if (!strcmp(varname, "service_perfdata_file_processing_interval"))
    xpddefault_service_perfdata_file_processing_interval = strtoul(varvalue, NULL, 0);
  else if (!strcmp(varname, "host_perfdata_file_processing_command"))
    xpddefault_host_perfdata_file_processing_command = my_strdup(varvalue);
  else if (!strcmp(varname, "service_perfdata_file_processing_command"))
    xpddefault_service_perfdata_file_processing_command = my_strdup(varvalue);

  /* free memory */
  delete[] varname;
  delete[] varvalue;

  return (OK);
}

/******************************************************************/
/************** INITIALIZATION & CLEANUP FUNCTIONS ****************/
/******************************************************************/

/* initializes performance data */
int xpddefault_initialize_performance_data(char* config_file) {
  char* buffer = NULL;
  char* temp_buffer = NULL;
  char* temp_command_name = NULL;
  command* temp_command = NULL;
  time_t current_time;
  nagios_macros* mac;

  mac = get_global_macros();
  time(&current_time);

  /* reset vars */
  xpddefault_host_perfdata_command_ptr = NULL;
  xpddefault_service_perfdata_command_ptr = NULL;
  xpddefault_host_perfdata_file_processing_command_ptr = NULL;
  xpddefault_service_perfdata_file_processing_command_ptr = NULL;

  /* grab config info from main config file */
  xpddefault_grab_config_info(config_file);

  /* make sure we have some templates defined */
  if (xpddefault_host_perfdata_file_template == NULL)
    xpddefault_host_perfdata_file_template = my_strdup(DEFAULT_HOST_PERFDATA_FILE_TEMPLATE);
  if (xpddefault_service_perfdata_file_template == NULL)
    xpddefault_service_perfdata_file_template = my_strdup(DEFAULT_SERVICE_PERFDATA_FILE_TEMPLATE);

  /* process special chars in templates */
  xpddefault_preprocess_file_templates(xpddefault_host_perfdata_file_template);
  xpddefault_preprocess_file_templates(xpddefault_service_perfdata_file_template);

  /* open the performance data files */
  xpddefault_open_host_perfdata_file();
  xpddefault_open_service_perfdata_file();

  /* verify that performance data commands are valid */
  if (xpddefault_host_perfdata_command != NULL) {
    temp_buffer = my_strdup(xpddefault_host_perfdata_command);

    /* get the command name, leave any arguments behind */
    temp_command_name = my_strtok(temp_buffer, "!");

    if ((temp_command = find_command(temp_command_name)) == NULL) {
      logit(NSLOG_RUNTIME_WARNING, TRUE,
            "Warning: Host performance command '%s' was not found - host performance data will not be processed!\n",
            temp_command_name);

      delete[] xpddefault_host_perfdata_command;
      xpddefault_host_perfdata_command = NULL;
    }

    delete[] temp_buffer;

    /* save the command pointer for later */
    xpddefault_host_perfdata_command_ptr = temp_command;
  }

  if (xpddefault_service_perfdata_command != NULL) {
    temp_buffer = my_strdup(xpddefault_service_perfdata_command);

    /* get the command name, leave any arguments behind */
    temp_command_name = my_strtok(temp_buffer, "!");

    if ((temp_command = find_command(temp_command_name)) == NULL) {
      logit(NSLOG_RUNTIME_WARNING, TRUE,
            "Warning: Service performance command '%s' was not found - service performance data will not be processed!\n",
            temp_command_name);

      delete[] xpddefault_service_perfdata_command;
      xpddefault_service_perfdata_command = NULL;
    }

    /* free memory */
    delete[] temp_buffer;

    /* save the command pointer for later */
    xpddefault_service_perfdata_command_ptr = temp_command;
  }

  if (xpddefault_host_perfdata_file_processing_command != NULL) {
    temp_buffer = my_strdup(xpddefault_host_perfdata_file_processing_command);

    /* get the command name, leave any arguments behind */
    temp_command_name = my_strtok(temp_buffer, "!");
    if ((temp_command = find_command(temp_command_name)) == NULL) {
      logit(NSLOG_RUNTIME_WARNING, TRUE,
            "Warning: Host performance file processing command '%s' was not found - host performance data file will not be processed!\n",
            temp_command_name);

      delete[] xpddefault_host_perfdata_file_processing_command;
      xpddefault_host_perfdata_file_processing_command = NULL;
    }

    /* free memory */
    delete[] temp_buffer;

    /* save the command pointer for later */
    xpddefault_host_perfdata_file_processing_command_ptr = temp_command;
  }

  if (xpddefault_service_perfdata_file_processing_command != NULL) {
    temp_buffer = my_strdup(xpddefault_service_perfdata_file_processing_command);

    /* get the command name, leave any arguments behind */
    temp_command_name = my_strtok(temp_buffer, "!");
    if ((temp_command = find_command(temp_command_name)) == NULL) {
      logit(NSLOG_RUNTIME_WARNING, TRUE,
            "Warning: Service performance file processing command '%s' was not found - service performance data file will not be processed!\n",
            temp_command_name);

      delete[] xpddefault_service_perfdata_file_processing_command;
      xpddefault_service_perfdata_file_processing_command = NULL;
    }

    /* free memory */
    delete[] temp_buffer;

    /* save the command pointer for later */
    xpddefault_service_perfdata_file_processing_command_ptr = temp_command;
  }

  /* periodically process the host perfdata file */
  if (xpddefault_host_perfdata_file_processing_interval > 0
      && xpddefault_host_perfdata_file_processing_command != NULL)
    schedule_new_event(EVENT_USER_FUNCTION,
		       TRUE,
                       current_time + xpddefault_host_perfdata_file_processing_interval,
                       TRUE,
                       xpddefault_host_perfdata_file_processing_interval,
                       NULL,
		       TRUE,
                       (void*)xpddefault_process_host_perfdata_file,
                       NULL,
		       0);

  /* periodically process the service perfdata file */
  if (xpddefault_service_perfdata_file_processing_interval > 0
      && xpddefault_service_perfdata_file_processing_command != NULL)
    schedule_new_event(EVENT_USER_FUNCTION,
		       TRUE,
                       current_time + xpddefault_service_perfdata_file_processing_interval,
                       TRUE,
                       xpddefault_service_perfdata_file_processing_interval,
                       NULL,
		       TRUE,
                       (void*)xpddefault_process_service_perfdata_file,
                       NULL,
		       0);

  /* save the host perf data file macro */
  delete[] mac->x[MACRO_HOSTPERFDATAFILE];
  if (xpddefault_host_perfdata_file != NULL) {
    mac->x[MACRO_HOSTPERFDATAFILE] = my_strdup(xpddefault_host_perfdata_file);
    strip(mac->x[MACRO_HOSTPERFDATAFILE]);
  }
  else
    mac->x[MACRO_HOSTPERFDATAFILE] = NULL;

  /* save the service perf data file macro */
  delete[] mac->x[MACRO_SERVICEPERFDATAFILE];
  if (xpddefault_service_perfdata_file != NULL) {
    mac->x[MACRO_SERVICEPERFDATAFILE] = my_strdup(xpddefault_service_perfdata_file);
    strip(mac->x[MACRO_SERVICEPERFDATAFILE]);
  }
  else
    mac->x[MACRO_SERVICEPERFDATAFILE] = NULL;

  /* free memory */
  delete[] buffer;

  return (OK);
}

/* cleans up performance data */
int xpddefault_cleanup_performance_data(char* config_file) {
  (void)config_file;

  /* free memory */
  delete[] xpddefault_host_perfdata_command;
  delete[] xpddefault_service_perfdata_command;
  delete[] xpddefault_host_perfdata_file_template;
  delete[] xpddefault_service_perfdata_file_template;
  delete[] xpddefault_host_perfdata_file;
  delete[] xpddefault_service_perfdata_file;
  delete[] xpddefault_host_perfdata_file_processing_command;
  delete[] xpddefault_service_perfdata_file_processing_command;

  xpddefault_host_perfdata_command = NULL;
  xpddefault_service_perfdata_command = NULL;
  xpddefault_host_perfdata_file_template = NULL;
  xpddefault_service_perfdata_file_template = NULL;
  xpddefault_host_perfdata_file = NULL;
  xpddefault_service_perfdata_file = NULL;
  xpddefault_host_perfdata_file_processing_command = NULL;
  xpddefault_service_perfdata_file_processing_command = NULL;

  /* close the files */
  xpddefault_close_host_perfdata_file();
  xpddefault_close_service_perfdata_file();

  return (OK);
}

/******************************************************************/
/****************** PERFORMANCE DATA FUNCTIONS ********************/
/******************************************************************/

/* updates service performance data */
int xpddefault_update_service_performance_data(service* svc) {
  nagios_macros mac;
  host* hst;

  /*
   * bail early if we've got nothing to do so we don't spend a lot
   * of time calculating macros that never get used
   */
  if (!svc || !svc->perf_data || !*svc->perf_data)
    return (OK);
  if ((!xpddefault_service_perfdata_fp
       || !xpddefault_service_perfdata_file_template)
      && !xpddefault_service_perfdata_command)
    return (OK);

  /*
   * we know we've got some work to do, so grab the necessary
   * macros and get busy
   */
  memset(&mac, 0, sizeof(mac));
  hst = find_host(svc->host_name);
  grab_host_macros(&mac, hst);
  grab_service_macros(&mac, svc);

  /* run the performance data command */
  xpddefault_run_service_performance_data_command(&mac, svc);

  /* get rid of used memory we won't need anymore */
  clear_argv_macros(&mac);

  /* update the performance data file */
  xpddefault_update_service_performance_data_file(&mac, svc);

  /* now free() it all */
  clear_volatile_macros(&mac);

  return (OK);
}


/* updates host performance data */
int xpddefault_update_host_performance_data(host* hst) {
  nagios_macros mac;

  /*
   * bail early if we've got nothing to do so we don't spend a lot
   * of time calculating macros that never get used
   */
  if (!hst || !hst->perf_data || !*hst->perf_data)
    return (OK);
  if ((!xpddefault_host_perfdata_fp
       || !xpddefault_host_perfdata_file_template)
      && !xpddefault_host_perfdata_command)
    return (OK);

  /* set up macros and get to work */
  memset(&mac, 0, sizeof(mac));
  grab_host_macros(&mac, hst);

  /* run the performance data command*/
  xpddefault_run_host_performance_data_command(&mac, hst);

  /* no more commands to run, so we won't need this any more */
  clear_argv_macros(&mac);

  /* update the performance data file */
  xpddefault_update_host_performance_data_file(&mac, hst);

  /* free() all */
  clear_volatile_macros(&mac);

  return (OK);
}

/******************************************************************/
/************** PERFORMANCE DATA COMMAND FUNCTIONS ****************/
/******************************************************************/

/* runs the service performance data command */
int xpddefault_run_service_performance_data_command(nagios_macros* mac, service* svc) {
  char* raw_command_line = NULL;
  char* processed_command_line = NULL;
  int early_timeout = FALSE;
  double exectime;
  int result = OK;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

  log_debug_info(DEBUGL_FUNCTIONS, 0, "run_service_performance_data_command()\n");

  if (svc == NULL)
    return (ERROR);

  /* we don't have a command */
  if (xpddefault_service_perfdata_command == NULL)
    return (OK);

  /* get the raw command line */
  get_raw_command_line_r(mac,
			 xpddefault_service_perfdata_command_ptr,
                         xpddefault_service_perfdata_command,
                         &raw_command_line, macro_options);
  if (raw_command_line == NULL)
    return (ERROR);

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Raw service performance data command line: %s\n",
                 raw_command_line);

  /* process any macros in the raw command line */
  process_macros_r(mac, raw_command_line, &processed_command_line, macro_options);
  if (processed_command_line == NULL)
    return (ERROR);

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Processed service performance data command line: %s\n",
                 processed_command_line);

  /* run the command */
  my_system_r(mac, processed_command_line, xpddefault_perfdata_timeout,
              &early_timeout, &exectime, NULL, 0);

  /* check to see if the command timed out */
  if (early_timeout == TRUE)
    logit(NSLOG_RUNTIME_WARNING, TRUE,
          "Warning: Service performance data command '%s' for service '%s' on host '%s' timed out after %d seconds\n",
          processed_command_line, svc->description, svc->host_name, xpddefault_perfdata_timeout);

  /* free memory */
  delete[] raw_command_line;
  delete[] processed_command_line;

  return (result);
}

/* runs the host performance data command */
int xpddefault_run_host_performance_data_command(nagios_macros* mac, host* hst) {
  char* raw_command_line = NULL;
  char* processed_command_line = NULL;
  int early_timeout = FALSE;
  double exectime;
  int result = OK;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;

  log_debug_info(DEBUGL_FUNCTIONS, 0, "run_host_performance_data_command()\n");

  if (hst == NULL)
    return (ERROR);

  /* we don't have a command */
  if (xpddefault_host_perfdata_command == NULL)
    return (OK);

  /* get the raw command line */
  get_raw_command_line_r(mac,
			 xpddefault_host_perfdata_command_ptr,
                         xpddefault_host_perfdata_command,
                         &raw_command_line, macro_options);
  if (raw_command_line == NULL)
    return (ERROR);

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Raw host performance data command line: %s\n",
                 raw_command_line);

  /* process any macros in the raw command line */
  process_macros_r(mac, raw_command_line, &processed_command_line, macro_options);

  log_debug_info(DEBUGL_PERFDATA, 2, "Processed host performance data command line: %s\n",
                 processed_command_line);

  /* run the command */
  my_system_r(mac,
	      processed_command_line,
	      xpddefault_perfdata_timeout,
              &early_timeout,
	      &exectime,
	      NULL,
	      0);
  if (processed_command_line == NULL)
    return (ERROR);

  /* check to see if the command timed out */
  if (early_timeout == TRUE)
    logit(NSLOG_RUNTIME_WARNING, TRUE,
          "Warning: Host performance data command '%s' for host '%s' timed out after %d seconds\n",
          processed_command_line, hst->name,
          xpddefault_perfdata_timeout);

  /* free memory */
  delete[] raw_command_line;
  delete[] processed_command_line;

  return (result);
}

/******************************************************************/
/**************** FILE PERFORMANCE DATA FUNCTIONS *****************/
/******************************************************************/

/* open the host performance data file for writing */
int xpddefault_open_host_perfdata_file(void) {
  if (xpddefault_host_perfdata_file != NULL) {
    if (xpddefault_host_perfdata_file_pipe == TRUE) {
      /* must open read-write to avoid failure if the other end isn't ready yet */
      xpddefault_host_perfdata_fd = open(xpddefault_host_perfdata_file, O_NONBLOCK | O_RDWR);
      xpddefault_host_perfdata_fp = fdopen(xpddefault_host_perfdata_fd, "w");
    }
    else
      xpddefault_host_perfdata_fp = fopen(xpddefault_host_perfdata_file,
					  (xpddefault_host_perfdata_file_append == TRUE) ? "a" : "w");

    if (xpddefault_host_perfdata_fp == NULL) {
      logit(NSLOG_RUNTIME_WARNING, TRUE,
            "Warning: File '%s' could not be opened - host performance data will not be written to file!\n",
            xpddefault_host_perfdata_file);

      return (ERROR);
    }
  }

  return (OK);
}

/* open the service performance data file for writing */
int xpddefault_open_service_perfdata_file(void) {
  if (xpddefault_service_perfdata_file != NULL) {
    if (xpddefault_service_perfdata_file_pipe == TRUE) {
      /* must open read-write to avoid failure if the other end isn't ready yet */
      xpddefault_service_perfdata_fd = open(xpddefault_service_perfdata_file, O_NONBLOCK | O_RDWR);
      xpddefault_service_perfdata_fp = fdopen(xpddefault_service_perfdata_fd, "w");
    }
    else
      xpddefault_service_perfdata_fp =
	fopen(xpddefault_service_perfdata_file,
	      (xpddefault_service_perfdata_file_append == TRUE) ? "a" : "w");

    if (xpddefault_service_perfdata_fp == NULL) {
      logit(NSLOG_RUNTIME_WARNING, TRUE,
            "Warning: File '%s' could not be opened - service performance data will not be written to file!\n",
            xpddefault_service_perfdata_file);

      return (ERROR);
    }
  }

  return (OK);
}

/* close the host performance data file */
int xpddefault_close_host_perfdata_file(void) {
  if (xpddefault_host_perfdata_fp != NULL)
    fclose(xpddefault_host_perfdata_fp);
  if (xpddefault_host_perfdata_fd >= 0) {
    close(xpddefault_host_perfdata_fd);
    xpddefault_host_perfdata_fd = -1;
  }

  return (OK);
}

/* close the service performance data file */
int xpddefault_close_service_perfdata_file(void) {
  if (xpddefault_service_perfdata_fp != NULL)
    fclose(xpddefault_service_perfdata_fp);
  if (xpddefault_service_perfdata_fd >= 0) {
    close(xpddefault_service_perfdata_fd);
    xpddefault_service_perfdata_fd = -1;
  }

  return (OK);
}

/* processes delimiter characters in templates */
void xpddefault_preprocess_file_templates(char* tmpl) {
  char* tempbuf;
  size_t x = 0;
  size_t y = 0;

  /* allocate temporary buffer */
  tempbuf = new char[strlen(tmpl) + 1];
  strcpy(tempbuf, "");

  for (x = 0, y = 0; x < strlen(tmpl); x++, y++) {
    if (tmpl[x] == '\\') {
      if (tmpl[x + 1] == 't') {
        tempbuf[y] = '\t';
        x++;
      }
      else if (tmpl[x + 1] == 'r') {
        tempbuf[y] = '\r';
        x++;
      }
      else if (tmpl[x + 1] == 'n') {
        tempbuf[y] = '\n';
        x++;
      }
      else
        tempbuf[y] = tmpl[x];
    }
    else
      tempbuf[y] = tmpl[x];
  }
  tempbuf[y] = '\x0';

  strcpy(tmpl, tempbuf);
  delete[] tempbuf;
}

/* updates service performance data file */
int xpddefault_update_service_performance_data_file(nagios_macros* mac, service* svc) {
  char* raw_output = NULL;
  char* processed_output = NULL;
  int result = OK;

  log_debug_info(DEBUGL_FUNCTIONS, 0, "update_service_performance_data_file()\n");

  if (svc == NULL)
    return (ERROR);

  /* we don't have a file to write to */
  if (xpddefault_service_perfdata_fp == NULL
      || xpddefault_service_perfdata_file_template == NULL)
    return (OK);

  /* get the raw line to write */
  raw_output = my_strdup(xpddefault_service_perfdata_file_template);

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Raw service performance data file output: %s\n",
                 raw_output);

  /* process any macros in the raw output line */
  process_macros_r(mac, raw_output, &processed_output, 0);
  if (processed_output == NULL)
    return (ERROR);

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Processed service performance data file output: %s\n",
                 processed_output);

  /* lock, write to and unlock host performance data file */
  pthread_mutex_lock(&xpddefault_service_perfdata_fp_lock);
  fputs(processed_output, xpddefault_service_perfdata_fp);
  fputc('\n', xpddefault_service_perfdata_fp);
  fflush(xpddefault_service_perfdata_fp);
  pthread_mutex_unlock(&xpddefault_service_perfdata_fp_lock);

  /* free memory */
  delete[] raw_output;
  delete[] processed_output;

  return (result);
}

/* updates host performance data file */
int xpddefault_update_host_performance_data_file(nagios_macros* mac, host* hst) {
  char* raw_output = NULL;
  char* processed_output = NULL;
  int result = OK;

  log_debug_info(DEBUGL_FUNCTIONS, 0, "update_host_performance_data_file()\n");

  if (hst == NULL)
    return (ERROR);

  /* we don't have a host perfdata file */
  if (xpddefault_host_perfdata_fp == NULL
      || xpddefault_host_perfdata_file_template == NULL)
    return (OK);

  /* get the raw output */
  raw_output = my_strdup(xpddefault_host_perfdata_file_template);

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Raw host performance file output: %s\n", raw_output);

  /* process any macros in the raw output */
  process_macros_r(mac, raw_output, &processed_output, 0);
  if (processed_output == NULL)
    return (ERROR);

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Processed host performance data file output: %s\n",
                 processed_output);

  /* lock, write to and unlock host performance data file */
  pthread_mutex_lock(&xpddefault_host_perfdata_fp_lock);
  fputs(processed_output, xpddefault_host_perfdata_fp);
  fputc('\n', xpddefault_host_perfdata_fp);
  fflush(xpddefault_host_perfdata_fp);
  pthread_mutex_unlock(&xpddefault_host_perfdata_fp_lock);

  /* free memory */
  delete[] raw_output;
  delete[] processed_output;

  return (result);
}

/* periodically process the host perf data file */
int xpddefault_process_host_perfdata_file(void) {
  char* raw_command_line = NULL;
  char* processed_command_line = NULL;
  int early_timeout = FALSE;
  double exectime = 0.0;
  int result = OK;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
  nagios_macros mac;

  log_debug_info(DEBUGL_FUNCTIONS, 0, "process_host_perfdata_file()\n");

  /* we don't have a command */
  if (xpddefault_host_perfdata_file_processing_command == NULL)
    return (OK);

  /* init macros */
  memset(&mac, 0, sizeof(mac));

  /* get the raw command line */
  get_raw_command_line_r(&mac,
                         xpddefault_host_perfdata_file_processing_command_ptr,
                         xpddefault_host_perfdata_file_processing_command,
                         &raw_command_line, macro_options);
  if (raw_command_line == NULL) {
    clear_volatile_macros(&mac);
    return (ERROR);
  }

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Raw host performance data file processing command line: %s\n",
                 raw_command_line);

  /* process any macros in the raw command line */
  process_macros_r(&mac, raw_command_line, &processed_command_line, macro_options);
  if (processed_command_line == NULL) {
    clear_volatile_macros(&mac);
    return (ERROR);
  }

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Processed host performance data file processing command line: %s\n",
                 processed_command_line);

  /* lock and close the performance data file */
  pthread_mutex_lock(&xpddefault_host_perfdata_fp_lock);
  xpddefault_close_host_perfdata_file();

  /* run the command */
  my_system_r(&mac,
	      processed_command_line,
	      xpddefault_perfdata_timeout,
              &early_timeout,
	      &exectime,
	      NULL,
	      0);
  clear_volatile_macros(&mac);

  /* re-open and unlock the performance data file */
  xpddefault_open_host_perfdata_file();
  pthread_mutex_unlock(&xpddefault_host_perfdata_fp_lock);

  /* check to see if the command timed out */
  if (early_timeout == TRUE)
    logit(NSLOG_RUNTIME_WARNING, TRUE,
          "Warning: Host performance data file processing command '%s' timed out after %d seconds\n",
          processed_command_line, xpddefault_perfdata_timeout);

  /* free memory */
  delete[] raw_command_line;
  delete[] processed_command_line;

  return (result);
}

/* periodically process the service perf data file */
int xpddefault_process_service_perfdata_file(void) {
  char* raw_command_line = NULL;
  char* processed_command_line = NULL;
  int early_timeout = FALSE;
  double exectime = 0.0;
  int result = OK;
  int macro_options = STRIP_ILLEGAL_MACRO_CHARS | ESCAPE_MACRO_CHARS;
  nagios_macros mac;

  log_debug_info(DEBUGL_FUNCTIONS, 0, "process_service_perfdata_file()\n");

  /* we don't have a command */
  if (xpddefault_service_perfdata_file_processing_command == NULL)
    return (OK);

  /* init macros */
  memset(&mac, 0, sizeof(mac));

  /* get the raw command line */
  get_raw_command_line_r(&mac,
                         xpddefault_service_perfdata_file_processing_command_ptr,
                         xpddefault_service_perfdata_file_processing_command,
                         &raw_command_line, macro_options);
  if (raw_command_line == NULL) {
    clear_volatile_macros(&mac);
    return (ERROR);
  }

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Raw service performance data file processing command line: %s\n",
                 raw_command_line);

  /* process any macros in the raw command line */
  process_macros_r(&mac, raw_command_line, &processed_command_line, macro_options);
  if (processed_command_line == NULL) {
    clear_volatile_macros(&mac);
    return (ERROR);
  }

  log_debug_info(DEBUGL_PERFDATA, 2,
                 "Processed service performance data file processing command line: %s\n",
                 processed_command_line);

  /* lock and close the performance data file */
  pthread_mutex_lock(&xpddefault_service_perfdata_fp_lock);
  xpddefault_close_service_perfdata_file();

  /* run the command */
  my_system_r(&mac,
	      processed_command_line,
	      xpddefault_perfdata_timeout,
              &early_timeout,
	      &exectime,
	      NULL,
	      0);

  /* re-open and unlock the performance data file */
  xpddefault_open_service_perfdata_file();
  pthread_mutex_unlock(&xpddefault_service_perfdata_fp_lock);

  clear_volatile_macros(&mac);

  /* check to see if the command timed out */
  if (early_timeout == TRUE)
    logit(NSLOG_RUNTIME_WARNING, TRUE,
          "Warning: Service performance data file processing command '%s' timed out after %d seconds\n",
          processed_command_line, xpddefault_perfdata_timeout);

  /* free memory */
  delete[] raw_command_line;
  delete[] processed_command_line;

  return (result);
}
