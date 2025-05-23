/**
 * Copyright 2011-2013,2017 Centreon
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
#ifndef CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH
#define CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH

#include "com/centreon/engine/configuration/applier/state.hh"

#include "common/engine_conf/hostdependency_helper.hh"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class hostdependency;
class state;

namespace applier {
class hostdependency {
 public:
  hostdependency() = default;
  hostdependency(const hostdependency&) = delete;
  ~hostdependency() noexcept = default;
  hostdependency& operator=(const hostdependency&) = delete;
  void add_object(const configuration::Hostdependency& obj);
  void modify_object(configuration::Hostdependency* to_modify,
                     const configuration::Hostdependency& new_obj);
  void remove_object(ssize_t idx);
  void expand_objects(configuration::State& s);
  void resolve_object(const configuration::Hostdependency& obj, error_cnt& err);
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH
