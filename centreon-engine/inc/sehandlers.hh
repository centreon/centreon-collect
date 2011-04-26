/*
** Copyright 2002-2006 Ethan Galstad
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

#ifndef CCE_SEHANDLERS_HH
# define CCE_SEHANDLERS_HH

# include "macros.hh"
# include "objects.hh"

# ifdef __cplusplus
extern "C" {
# endif

// Event Handler Types
# define HOST_EVENTHANDLER           0
# define SERVICE_EVENTHANDLER        1
# define GLOBAL_HOST_EVENTHANDLER    2
# define GLOBAL_SERVICE_EVENTHANDLER 3

// Event Handler Functions
int obsessive_compulsive_service_check_processor(service* svc);         // distributed monitoring craziness...
int obsessive_compulsive_host_check_processor(host* hst);               // distributed monitoring craziness...
int handle_service_event(service* svc);                                 // top level service event logic
int run_global_service_event_handler(nagios_macros* mac, service* svc); // runs the global service event handler
int run_service_event_handler(nagios_macros* mac, service* svc);        // runs the event handler for a specific service
int handle_host_event(host* hst);                                       // top level host event logic
int run_global_host_event_handler(nagios_macros* mac, host* hst);       // runs the global host event handler
int run_host_event_handler(nagios_macros* mac, host* hst);              // runs the event handler for a specific host
int handle_host_state(host* hst);                                       // top level host state handler

# ifdef __cplusplus
}
# endif

#endif // !CCE_SEHANDLERS_HH
