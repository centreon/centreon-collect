/**
 * Copyright 2011-2013,2017,2023 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_CONNECTOR_HH
#define CCE_CONFIGURATION_APPLIER_CONNECTOR_HH

#include "common/configuration/state.pb.h"
namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class connector;
class state;

namespace applier {
class connector {
 public:
  /**
   * @brief Default constructor.
   */
  connector() = default;
  /**
   * @brief Destructor.
   */
  ~connector() noexcept = default;
  connector(const connector&) = delete;
  connector& operator=(const connector&) = delete;
#ifdef LEGACY_CONF
  void add_object(configuration::connector const& obj);
  void modify_object(const configuration::connector& obj);
  void remove_object(configuration::connector const& obj);
  void expand_objects(configuration::state& s);
  void resolve_object(configuration::connector const& obj);
#else
  void add_object(const configuration::Connector& obj);
  void modify_object(configuration::Connector* to_modify,
                     const configuration::Connector& new_obj);
  void remove_object(ssize_t idx);
  void expand_objects(configuration::State& s);
  void resolve_object(const configuration::Connector& obj);
#endif
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_CONNECTOR_HH
