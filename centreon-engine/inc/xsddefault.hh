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

#ifndef CCS_XSDDEFAULT_HH
# define CCS_XSDDEFAULT_HH

# ifdef __cplusplus
extern "C" {
# endif

int xsddefault_initialize_status_data(char* config_file);
int xsddefault_cleanup_status_data(char* config_file, int delete_status_data);
int xsddefault_save_status_data(void);

int xsddefault_grab_config_info(char* config_file);
int xsddefault_grab_config_directives(char* input);

# ifdef __cplusplus
}
# endif

#endif // !CCS_XSDDEFAULT_HH
