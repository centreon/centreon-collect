/**
 * Copyright 2011-2013,2016-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include "com/centreon/engine/configuration/applier/macros.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/macros.hh"
using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

/**
 *  @brief Is the key of this user macro old-style?
 *
 *  i.e USERn where n is a number.
 *
 *  @param[in] key   The key.
 *  @param[out] val  The parsed value n, if applicable.
 *
 *  @return  True if the key is old-style and has been parsed succesfully.
 */
static bool is_old_style_user_macro(std::string const& key, unsigned int& val) {
  if (!absl::StartsWith(key, "USER"))
    return false;
  return absl::SimpleAtoi(key.substr(4), &val);
}

/**
 *  Apply new configuration.
 *
 *  @param[in] config The new configuration.
 */
void applier::macros::apply(configuration::State& pb_config) {
  auto& users = applier::state::instance().user_macros();
  users.clear();

  for (auto& p : pb_config.users())
    users[p.first] = p.second;

  // Save old style user macros into old style structures.
  for (auto& p : users) {
    unsigned int val = 1;
    if (is_old_style_user_macro(p.first, val))
      _set_macros_user(val - 1, p.second);
  }
}

/**
 *  Get the singleton instance of macros applier.
 *
 *  @return Singleton instance.
 */
applier::macros& applier::macros::instance() {
  static applier::macros instance;
  return instance;
}

void applier::macros::clear() {
  for (unsigned int i(0); i < MAX_USER_MACROS; ++i) {
    macro_user[i] = "";
  }
}

/**
 *  Default constructor.
 */
applier::macros::macros() {}

/**
 *  Destructor.
 */
applier::macros::~macros() noexcept {
  for (unsigned int i(0); i < MAX_USER_MACROS; ++i) {
    macro_user[i] = "";
  }
}

/**
 *  Set the user macros.
 *
 *  @param[in] idx   The index of the user macro to set.
 *  @param[in] value The value of the macro.
 */
void applier::macros::_set_macros_user(unsigned int idx,
                                       std::string const& value) {
  if (idx >= MAX_USER_MACROS)
    throw(engine_error() << "Invalid index of user macro: " << idx);
  if (macro_user[idx] != value)
    macro_user[idx] = value;
}
