/**
 * Copyright 2011-2013,2017-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */
#ifndef CCE_CONFIGURATION_APPLIER_HOSTESCALATION_HH
#define CCE_CONFIGURATION_APPLIER_HOSTESCALATION_HH
#include "com/centreon/engine/configuration/applier/state.hh"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class hostescalation;
class state;

namespace applier {
class hostescalation {
  void _expand_hosts(std::set<std::string> const& h,
                     std::set<std::string> const& hg,
                     configuration::state const& s,
                     std::set<std::string>& expanded);
  void _inherits_special_vars(configuration::hostescalation& obj,
                              configuration::state& s);

 public:
  hostescalation() = default;
  ~hostescalation() noexcept = default;
  hostescalation(hostescalation const&) = delete;
  hostescalation& operator=(hostescalation const&) = delete;
  void add_object(const configuration::hostescalation& obj);
  void modify_object(configuration::hostescalation const& obj);
  void remove_object(configuration::hostescalation const& obj);
  void expand_objects(configuration::state& s);
  void resolve_object(configuration::hostescalation const& obj, error_cnt& err);
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_HOSTESCALATION_HH
