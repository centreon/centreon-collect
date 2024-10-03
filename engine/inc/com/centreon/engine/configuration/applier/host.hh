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
#ifndef CCE_CONFIGURATION_APPLIER_HOST_HH
#define CCE_CONFIGURATION_APPLIER_HOST_HH
#include "com/centreon/engine/configuration/applier/state.hh"

#ifndef LEGACY_CONF
#include "common/engine_conf/host_helper.hh"
#endif

namespace com::centreon::engine::configuration {

#ifdef LEGACY_CONF
// Forward declarations.
class host;
class state;
#endif

namespace applier {
class host {
 public:
  host() = default;
  host(host const&) = delete;
  ~host() noexcept = default;
  host& operator=(host const&) = delete;
#ifdef LEGACY_CONF
  void add_object(const configuration::host& obj);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::host const& obj);
  void remove_object(configuration::host const& obj);
  void resolve_object(configuration::host const& obj, error_cnt& err);
#else
  void add_object(const configuration::Host& obj);
  void expand_objects(configuration::State& s);
  void modify_object(configuration::Host* old_obj,
                     const configuration::Host& new_obj);
  void remove_object(ssize_t idx);
  void resolve_object(const configuration::Host& obj, error_cnt& err);
#endif
};
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_HOST_HH
