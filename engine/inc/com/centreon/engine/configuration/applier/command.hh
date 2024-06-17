/**
 * Copyright 2011-2013,2017,2023-2024 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_COMMAND_HH
#define CCE_CONFIGURATION_APPLIER_COMMAND_HH

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
  command();

  command(command const&) = delete;
  command& operator=(command const&) = delete;

  ~command() noexcept;

  void add_object(configuration::command const& obj);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::command const& obj);
  void remove_object(configuration::command const& obj);
  void resolve_object(configuration::command const& obj);
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_COMMAND_HH
