/*
** Copyright 2011-2013,2017,2023 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_COMMAND_HH
#define CCE_CONFIGURATION_APPLIER_COMMAND_HH

#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

// Forward declarations.
namespace commands {
class command;
}

namespace configuration {
// Forward declarations.
class command;
class state;

namespace applier {
class command {
 public:
  /**
   * @brief Default constructor
   */
  command() = default;
  /**
   * @brief Destructor.
   */
  ~command() noexcept = default;
  command(const command&) = delete;
  command& operator=(const command&) = delete;
  void add_object(const configuration::Command& obj);
  void add_object(const configuration::command& obj);
  void expand_objects(configuration::State& s);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::Command* to_modify,
                     const configuration::Command& new_obj);
  void modify_object(const configuration::command& obj);
  void remove_object(ssize_t idx);
  void remove_object(const configuration::command& obj);
  void resolve_object(const configuration::Command& obj);
  void resolve_object(const configuration::command& obj);
};
}  // namespace applier
}  // namespace configuration

}

#endif  // !CCE_CONFIGURATION_APPLIER_COMMAND_HH
