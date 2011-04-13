/*
** Copyright 2002-2006 Ethan Galstad
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

#ifndef CCS_NEBMODULES_HH
# define CCS_NEBMODULES_HH

# ifdef HAVE_PTHREAD_HH
#  include <pthread.h>
# endif // !HAVE_PTHREAD_HH

# ifdef __cplusplus
extern "C" {
# endif

/***** MODULE VERSION INFORMATION *****/

# define NEB_API_VERSION(x) int __neb_api_version = x;
# define CURRENT_NEB_API_VERSION    3

/***** MODULE INFORMATION *****/

# define NEBMODULE_MODINFO_NUMITEMS  6
# define NEBMODULE_MODINFO_TITLE     0
# define NEBMODULE_MODINFO_AUTHOR    1
# define NEBMODULE_MODINFO_COPYRIGHT 2
# define NEBMODULE_MODINFO_VERSION   3
# define NEBMODULE_MODINFO_LICENSE   4
# define NEBMODULE_MODINFO_DESC      5

/***** MODULE LOAD/UNLOAD OPTIONS *****/

# define NEBMODULE_NORMAL_LOAD       0    /* module is being loaded normally */
# define NEBMODULE_REQUEST_UNLOAD    0    /* request module to unload (but don't force it) */
# define NEBMODULE_FORCE_UNLOAD      1    /* force module to unload */

/***** MODULES UNLOAD REASONS *****/

# define NEBMODULE_NEB_SHUTDOWN      1    /* event broker is shutting down */
# define NEBMODULE_NEB_RESTART       2    /* event broker is restarting */
# define NEBMODULE_ERROR_NO_INIT     3    /* _module_init() function was not found in module */
# define NEBMODULE_ERROR_BAD_INIT    4    /* _module_init() function returned a bad code */
# define NEBMODULE_ERROR_API_VERSION 5    /* module version is incompatible with current api */

/***** MODULE FUNCTIONS *****/
int neb_set_module_info(void*, int, char*);

/***** MODULE STRUCTURES *****/

// NEB module structure
typedef struct nebmodule_struct {
  char*                    filename;
  char*                    args;
  char*                    info[NEBMODULE_MODINFO_NUMITEMS];
  int                      should_be_loaded;
  int                      is_currently_loaded;
  void*                    module_handle;
  void*                    init_func;
  void*                    deinit_func;
#ifdef HAVE_PTHREAD_HH
  pthread_t                thread_id;
#endif // !HAVE_PTHREAD_HH
  struct nebmodule_struct* next;
}                          nebmodule;

# ifdef __cplusplus
}
# endif

#endif // !CCS_NEBMODULES_HH
