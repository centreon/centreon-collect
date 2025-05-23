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
#ifndef CCE_CONFIGURATION_APPLIER_SERVICE_HH
#define CCE_CONFIGURATION_APPLIER_SERVICE_HH
#include "com/centreon/engine/configuration/applier/state.hh"

#include "common/engine_conf/service_helper.hh"

namespace com::centreon::engine::configuration {

namespace applier {
class service {
 public:
  service() = default;
  service(service const&) = delete;
  ~service() noexcept = default;
  service& operator=(service const&) = delete;
  void add_object(const configuration::Service& obj);
  void expand_objects(configuration::State& s);
  void modify_object(configuration::Service* old_obj,
                     const configuration::Service& new_obj);
  void remove_object(ssize_t idx);
  void resolve_object(const configuration::Service& obj, error_cnt& err);

 private:
  void _expand_service_memberships(configuration::Service& obj,
                                   configuration::State& s);
  void _inherits_special_vars(configuration::Service& obj,
                              const configuration::State& s);
};
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICE_HH
