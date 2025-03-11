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
#include "com/centreon/engine/macros/misc.hh"
#include "com/centreon/engine/string.hh"

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
  if (::strncmp(key.c_str(), "USER", ::strlen("USER")) != 0)
    return (false);

  std::string rest = key.substr(4);
  // Super strict validation.
  for (size_t i = 0; i < rest.size(); ++i)
    if (rest[i] < '0' || rest[i] > '9')
      return (false);
  string::to(rest.c_str(), val);
  return (true);
}

/**
 *  Apply new configuration.
 *
 *  @param[in] config The new configuration.
 */
void applier::macros::apply(configuration::State& pb_config) {
  _set_macro(MACRO_ADMINEMAIL, pb_config.admin_email());
  _set_macro(MACRO_ADMINPAGER, pb_config.admin_pager());
  _set_macro(MACRO_COMMANDFILE, pb_config.command_file());
  _set_macro(MACRO_LOGFILE, pb_config.log_file());
  _set_macro(MACRO_MAINCONFIGFILE, pb_config.cfg_main());
  if (pb_config.resource_file().size() > 0)
    _set_macro(MACRO_RESOURCEFILE, pb_config.resource_file(0));
  _set_macro(MACRO_STATUSDATAFILE, pb_config.status_file());
  _set_macro(MACRO_RETENTIONDATAFILE, pb_config.state_retention_file());
  _set_macro(MACRO_POLLERNAME, pb_config.poller_name());
  _set_macro(MACRO_POLLERID, std::to_string(pb_config.poller_id()));

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
  clear_volatile_macros_r(_mac);
  free_macrox_names();

  for (unsigned int i(0); i < MAX_USER_MACROS; ++i) {
    macro_user[i] = "";
  }

  _mac = get_global_macros();
  init_macros();

  _set_macro(MACRO_TEMPFILE, "/tmp/centengine.tmp");
  _set_macro(MACRO_TEMPPATH, "/tmp");
}

/**
 *  Default constructor.
 */
applier::macros::macros() : _mac(get_global_macros()) {
  init_macros();

  _set_macro(MACRO_TEMPFILE, "/tmp/centengine.tmp");
  _set_macro(MACRO_TEMPPATH, "/tmp");
}

/**
 *  Destructor.
 */
applier::macros::~macros() throw() {
  clear_volatile_macros_r(_mac);
  free_macrox_names();

  for (unsigned int i(0); i < MAX_USER_MACROS; ++i) {
    macro_user[i] = "";
  }
}

/**
 *  Set the global macros.
 *
 *  @param[in] type  The type of macros to set.
 *  @param[in] value The value of the macro.
 */
void applier::macros::_set_macro(unsigned int type, std::string const& value) {
  if (type >= MACRO_X_COUNT)
    throw(engine_error() << "Invalid type of global macro: " << type);
  if (_mac->x[type] != value)
    _mac->x[type] = value;
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
