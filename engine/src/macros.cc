/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2024 Centreon
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

#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon::engine;
using namespace com::centreon::common::timeperiods;
using namespace com::centreon::engine::configuration::applier;

/******************************************************************/
/********************* MACRO STRING FUNCTIONS *********************/
/******************************************************************/

/**
 *  Remove the illegal characters from the value of a macro: the ASCII
 *  control characters and the illegal_output_chars of the configuration.
 *  The non-ASCII characters (Japanese, etc) are kept.
 *
 *  @param[in,out] value The value of the macro.
 */
void strip_illegal_macro_chars(std::string& value) {
  const std::string& illegal = pb_indexed_config.state().illegal_output_chars();
  value.erase(std::remove_if(value.begin(), value.end(),
                             [&illegal](char c) {
                               const unsigned char ch = c;
                               return ch < 32 || ch == 127 ||
                                      illegal.find(c) != std::string::npos;
                             }),
              value.end());
}
