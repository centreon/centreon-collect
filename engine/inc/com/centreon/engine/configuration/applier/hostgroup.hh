/**
 * Copyright 2011-2013,2017-2024 Centreon
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
#ifndef CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH
#define CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH

#include "com/centreon/engine/configuration/applier/state.hh"
#include "common/engine_conf/hostgroup_helper.hh"

namespace com::centreon::engine::configuration {

// Forward declarations.
class state;

namespace applier {
class hostgroup {
 public:
  hostgroup() = default;
  hostgroup(hostgroup const&) = delete;
  ~hostgroup() noexcept = default;
  hostgroup& operator=(hostgroup const& right) = delete;
  void add_object(const configuration::Hostgroup& obj);
  void expand_objects(configuration::State& s);
  void modify_object(configuration::Hostgroup* old_obj,
                     const configuration::Hostgroup& new_obj);
  template <typename Key>
  void remove_object(const std::pair<ssize_t, Key>& p);
  void resolve_object(const configuration::Hostgroup& obj, error_cnt& err);
};

template <>
void hostgroup::remove_object(const std::pair<ssize_t, std::string>& p);
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH
