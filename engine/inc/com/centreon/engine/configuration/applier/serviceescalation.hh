/**
 * Copyright 2011-2013,2017,2023-2024 Centreon
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
#ifndef CCE_CONFIGURATION_APPLIER_SERVICEESCALATION_HH
#define CCE_CONFIGURATION_APPLIER_SERVICEESCALATION_HH
#include "com/centreon/engine/configuration/applier/state.hh"

#ifndef LEGACY_CONF
#include "common/engine_conf/serviceescalation_helper.hh"
#endif

namespace com::centreon::engine::configuration {

// Forward declarations.
#ifdef LEGACY_CONF
class serviceescalation;
class state;
#endif

namespace applier {
class serviceescalation {
#ifdef LEGACY_CONF
  void _expand_services(
      std::list<std::string> const& hst,
      std::list<std::string> const& hg,
      std::list<std::string> const& svc,
      std::list<std::string> const& sg,
      configuration::state& s,
      std::set<std::pair<std::string, std::string> >& expanded);
  void _inherits_special_vars(configuration::serviceescalation& obj,
                              configuration::state const& s);
#endif

 public:
  serviceescalation() = default;
  serviceescalation(const serviceescalation&) = delete;
  ~serviceescalation() noexcept = default;
  serviceescalation& operator=(const serviceescalation&) = delete;
#ifdef LEGACY_CONF
  void add_object(const configuration::serviceescalation& obj);
  void modify_object(const configuration::serviceescalation& obj);
  void remove_object(const configuration::serviceescalation& obj);
  void expand_objects(configuration::state& s);
  void resolve_object(const configuration::serviceescalation& obj,
                      error_cnt& err);
#else
  void add_object(const configuration::Serviceescalation& obj);
  void modify_object(configuration::Serviceescalation* old_obj,
                     const configuration::Serviceescalation& new_obj);
  void remove_object(ssize_t idx);
  void expand_objects(configuration::State& s);
  void resolve_object(const configuration::Serviceescalation& obj,
                      error_cnt& err);
#endif
};
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICEESCALATION_HH
