/**
 * Copyright 2011-2024 Centreon
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
#ifndef CCE_OBJECTS_CONTACTGROUP_HH
#define CCE_OBJECTS_CONTACTGROUP_HH

#include <absl/container/flat_hash_map.h>
#include <list>
#include <memory>
#include <string>

#include "common/engine_conf/contactgroup_helper.hh"

/* Forward declaration. */
namespace com::centreon::engine {
class contact;
class contactgroup;

namespace configuration {
class contactgroup;
}
}  // namespace com::centreon::engine

using contactgroup_map =
    absl::flat_hash_map<std::string,
                        std::shared_ptr<com::centreon::engine::contactgroup>>;
using contactgroup_map_unsafe =
    absl::flat_hash_map<std::string, com::centreon::engine::contactgroup*>;
using contact_map_unsafe =
    absl::flat_hash_map<std::string, com::centreon::engine::contact*>;

namespace com::centreon::engine {

class contactgroup {
 public:
  contactgroup() = default;
#ifdef LEGACY_CONF
  contactgroup(configuration::contactgroup const& obj);
#else
  contactgroup(const configuration::Contactgroup& obj);
#endif
  virtual ~contactgroup();
  std::string const& get_name() const;
  std::string const& get_alias() const;
  void set_alias(std::string const& alias);
  void add_member(contact* cntct);
  void clear_members();
  contact_map_unsafe& get_members();
  contact_map_unsafe const& get_members() const;
  void resolve(uint32_t& w, uint32_t& e);

  contactgroup& operator=(contactgroup const& other);

  static contactgroup_map contactgroups;

 private:
  std::string _alias;
  contact_map_unsafe _members;
  std::string _name;
};

}  // namespace com::centreon::engine

std::ostream& operator<<(std::ostream& os, contactgroup_map_unsafe const& obj);

#endif  // !CCE_OBJECTS_CONTACTGROUP_HH
