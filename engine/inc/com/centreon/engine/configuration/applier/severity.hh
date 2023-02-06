/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_APPLIER_SEVERITY_HH
#define CCE_CONFIGURATION_APPLIER_SEVERITY_HH

#include "configuration/state-generated.pb.h"

namespace com::centreon::engine {

namespace configuration {
class severity;
class state;

namespace applier {
class severity {
 public:
  severity() = default;
  ~severity() noexcept = default;
  severity& operator=(const severity&) = delete;
  void add_object(const configuration::Severity& obj);
  void add_object(const configuration::severity& obj);
  void expand_objects(configuration::state& s);
  void modify_object(const configuration::severity& obj);
  void remove_object(ssize_t idx);
  void remove_object(const configuration::severity& obj);
  void resolve_object(const configuration::severity& obj);
};
}  // namespace applier
}  // namespace configuration

}

#endif  // !CCE_CONFIGURATION_APPLIER_SEVERITY_HH
