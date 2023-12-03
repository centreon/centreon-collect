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

#ifndef CCE_CONFIGURATION_APPLIER_CONTACT_HH
#define CCE_CONFIGURATION_APPLIER_CONTACT_HH

#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class contact;
class state;

namespace applier {
class contact {
 public:
  /**
   * @brief Default constructor.
   */
  contact() = default;

  /**
   * @brief Destructor
   */
  ~contact() noexcept = default;
  contact(contact const&) = delete;
  contact& operator=(const contact&) = delete;

  void expand_objects(configuration::State& s);
  void expand_objects(configuration::state& s);
#ifdef LEGACY_CONF
  void add_object(const configuration::contact& obj);
  void modify_object(const configuration::contact& obj);
  void remove_object(const configuration::contact& obj);
#else
  void add_object(const configuration::Contact& obj);
  void modify_object(configuration::Contact* to_modify,
                     const configuration::Contact& new_object);
  void remove_object(ssize_t idx);
#endif
  void resolve_object(const configuration::Contact& obj);
  void resolve_object(const configuration::contact& obj);
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_CONTACT_HH
