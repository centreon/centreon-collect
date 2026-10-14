/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2025 Centreon
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

#ifndef CCE_MACROS_RESOLVE_HH
#define CCE_MACROS_RESOLVE_HH

#include "engine/macros/macro_context.hh"
#include "engine/macros/descriptors.hh"

namespace com::centreon::engine::macros {

std::string grab_macro_value(macro_context& mac,
                             std::string_view macro_name,
                             bool strip_illegal);
std::string grab_macrox_value(macro_context& mac,
                              const descriptor& desc,
                              std::string_view arg1,
                              std::string_view arg2);

}  // namespace com::centreon::engine::macros

#endif  // !CCE_MACROS_RESOLVE_HH
