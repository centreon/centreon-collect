/*
** Copyright 2001-2006 Ethan Galstad
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

#ifndef _XDDDEFAULT_H
#define _XDDDEFAULT_H

#define XDDDEFAULT_NO_DATA          0
#define XDDDEFAULT_INFO_DATA        1
#define XDDDEFAULT_HOST_DATA        2
#define XDDDEFAULT_SERVICE_DATA     3

int xdddefault_initialize_downtime_data(char *);
int xdddefault_validate_downtime_data(void);
int xdddefault_cleanup_downtime_data(char *);

int xdddefault_save_downtime_data(void);
int xdddefault_add_new_host_downtime(char *,time_t,char *,char *,time_t,time_t,int,unsigned long,unsigned long,unsigned long *);
int xdddefault_add_new_service_downtime(char *,char *,time_t,char *,char *,time_t,time_t,int,unsigned long,unsigned long,unsigned long *);

int xdddefault_delete_host_downtime(unsigned long);
int xdddefault_delete_service_downtime(unsigned long);
int xdddefault_delete_downtime(int,unsigned long);

#endif
