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
#ifndef CCE_OBJECTS_HOSTGROUP_HH
#define CCE_OBJECTS_HOSTGROUP_HH

#include "com/centreon/engine/host.hh"

/* Forward declaration. */
namespace com::centreon::engine {
class hostgroup;
}

using hostgroup_map =
    absl::flat_hash_map<std::string,
                        std::shared_ptr<com::centreon::engine::hostgroup>>;

namespace com::centreon::engine {
class hostgroup {
 public:
  hostgroup(uint64_t id,
            std::string const& name,
            std::string const& alias,
            std::string const& notes,
            std::string const& notes_url,
            std::string const& action_url);

  uint64_t get_id() const;
  void set_id(uint64_t id);
  std::string const& get_group_name() const;
  void set_group_name(std::string const& group_name);
  std::string const& get_alias() const;
  void set_alias(std::string const& alias);
  std::string const& get_notes() const;
  void set_notes(std::string const& notes);
  std::string const& get_notes_url() const;
  void set_notes_url(std::string const& notes_url);
  std::string const& get_action_url() const;
  void set_action_url(std::string const& action_url);
  bool operator==(hostgroup const& obj) = delete;
  bool operator!=(hostgroup const& obj1) = delete;
  void resolve(uint32_t& w, uint32_t& e);

  host_map_unsafe members;

  static hostgroup_map hostgroups;

 private:
  uint64_t _id;
  std::string _group_name;
  std::string _alias;
  std::string _notes;
  std::string _notes_url;
  std::string _action_url;
};
}  // namespace com::centreon::engine

std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::hostgroup const& obj);

#endif  // !CCE_OBJECTS_HOSTGROUP_HH
