/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2025 Centreon
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

#ifndef CCE_MACROS_HH
#define CCE_MACROS_HH
#include "com/centreon/engine/macros/grab_host.hh"
#include "com/centreon/engine/macros/grab_service.hh"
#include "com/centreon/engine/macros/grab_value.hh"
#include "com/centreon/engine/macros/process.hh"

// cleans macros characters before insertion into output string
std::string clean_macro_chars(std::string const& macro, int options);

int grab_contact_macros_r(macro_context* mac,
                          com::centreon::engine::contact* cntct);

/* thread-safe version of the above */
int clear_argv_macros_r(macro_context* mac);
int clear_volatile_macros_r(macro_context* mac);
int clear_summary_macros_r(macro_context* mac);

#endif  // !CCE_MACROS_HH
