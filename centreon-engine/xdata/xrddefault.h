/*
** Copyright 1999-2006 Ethan Galstad
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

#ifndef _XRDDEFAULT_H
#define _XRDDEFAULT_H


#define XRDDEFAULT_NO_DATA               0
#define XRDDEFAULT_INFO_DATA             1
#define XRDDEFAULT_PROGRAMSTATUS_DATA    2
#define XRDDEFAULT_HOSTSTATUS_DATA       3
#define XRDDEFAULT_SERVICESTATUS_DATA    4
#define XRDDEFAULT_CONTACTSTATUS_DATA    5
#define XRDDEFAULT_HOSTCOMMENT_DATA      6
#define XRDDEFAULT_SERVICECOMMENT_DATA   7
#define XRDDEFAULT_HOSTDOWNTIME_DATA     8
#define XRDDEFAULT_SERVICEDOWNTIME_DATA  9

int xrddefault_initialize_retention_data(char *);
int xrddefault_cleanup_retention_data(char *);
int xrddefault_grab_config_info(char *);
int xrddefault_grab_config_directives(char *);
int xrddefault_save_state_information(void);        /* saves all host and service state information */
int xrddefault_read_state_information(void);        /* reads in initial host and service state information */

#endif
