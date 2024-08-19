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

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class service;
class state;

namespace applier {
class service {
 public:
  service();
  service(service const& right);
  ~service();
  service& operator=(service const& right);
  void add_object(configuration::service const& obj);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::service const& obj);
  void remove_object(configuration::service const& obj);
  void resolve_object(configuration::service const& obj, error_cnt& err);

 private:
  void _expand_service_memberships(configuration::service& obj,
                                   configuration::state& s);
  void _inherits_special_vars(configuration::service& obj,
                              configuration::state const& s);
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICE_HH
