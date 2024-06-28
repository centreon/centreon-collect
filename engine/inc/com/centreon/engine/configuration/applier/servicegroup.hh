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
#ifndef CCE_CONFIGURATION_APPLIER_SERVICEGROUP_HH
#define CCE_CONFIGURATION_APPLIER_SERVICEGROUP_HH

#include "com/centreon/engine/configuration/applier/state.hh"
#include "common/engine_legacy_conf/servicegroup.hh"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class state;

namespace applier {
class servicegroup {
 public:
  servicegroup();
  servicegroup(servicegroup const& right);
  ~servicegroup() throw();
  servicegroup& operator=(servicegroup const& right);
  void add_object(configuration::servicegroup const& obj);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::servicegroup const& obj);
  void remove_object(configuration::servicegroup const& obj);
  void resolve_object(configuration::servicegroup const& obj, error_cnt& err);

 private:
  typedef std::map<configuration::servicegroup::key_type,
                   configuration::servicegroup>
      resolved_set;

  void _resolve_members(configuration::servicegroup const& obj,
                        configuration::state const& s);

  resolved_set _resolved;
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICEGROUP_HH
