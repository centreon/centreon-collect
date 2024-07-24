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
#ifdef LEGACY_CONF
#include "common/engine_legacy_conf/hostgroup.hh"
#else
#include "common/engine_conf/hostgroup_helper.hh"
#endif

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
#ifdef LEGACY_CONF
  void add_object(configuration::hostgroup const& obj);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::hostgroup const& obj);
  void remove_object(configuration::hostgroup const& obj);
  void resolve_object(configuration::hostgroup const& obj, error_cnt& err);
#else
  void add_object(const configuration::Hostgroup& obj);
  void expand_objects(configuration::State& s);
  void modify_object(configuration::Hostgroup* old_obj,
                     const configuration::Hostgroup& new_obj);
  void remove_object(ssize_t idx);
  void resolve_object(const configuration::Hostgroup& obj, error_cnt& err);
#endif

 private:
#ifdef LEGACY_CONF
  typedef std::map<configuration::hostgroup::key_type, configuration::hostgroup>
      resolved_set;

  void _resolve_members(configuration::state& s,
                        configuration::hostgroup const& obj);

  resolved_set _resolved;
#endif
};
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH
