/*
** Copyright 2001-2006 Ethan Galstad
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

#ifndef XPDDEFAULT_H
# define XPDDEFAULT_H

# include "../include/objects.h"


# define DEFAULT_HOST_PERFDATA_FILE_TEMPLATE    "[HOSTPERFDATA]\t$TIMET$\t$HOSTNAME$\t$HOSTEXECUTIONTIME$\t$HOSTOUTPUT$\t$HOSTPERFDATA$"
# define DEFAULT_SERVICE_PERFDATA_FILE_TEMPLATE "[SERVICEPERFDATA]\t$TIMET$\t$HOSTNAME$\t$SERVICEDESC$\t$SERVICEEXECUTIONTIME$\t$SERVICELATENCY$\t$SERVICEOUTPUT$\t$SERVICEPERFDATA$"



int xpddefault_initialize_performance_data(char *);
int xpddefault_cleanup_performance_data(char *);
int xpddefault_grab_config_info(char *);
int xpddefault_grab_config_directives(char *);

int xpddefault_update_service_performance_data(service *);
int xpddefault_update_host_performance_data(host *);

int xpddefault_run_service_performance_data_command(nagios_macros *mac, service *);
int xpddefault_run_host_performance_data_command(nagios_macros *mac, host *);

int xpddefault_update_service_performance_data_file(nagios_macros *mac, service *);
int xpddefault_update_host_performance_data_file(nagios_macros *mac, host *);

int xpddefault_preprocess_file_templates(char *);

int xpddefault_open_host_perfdata_file(void);
int xpddefault_open_service_perfdata_file(void);
int xpddefault_close_host_perfdata_file(void);
int xpddefault_close_service_perfdata_file(void);

int xpddefault_process_host_perfdata_file(void);
int xpddefault_process_service_perfdata_file(void);

#endif /* !XPDDEFAULT_H */
