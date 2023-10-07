/*
 * Copyright 2022-2023 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_APPLIER_TAG_HH
#define CCE_CONFIGURATION_APPLIER_TAG_HH
#include "common/configuration/state-generated.pb.h"
#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

namespace configuration {
class tag;
class state;

namespace applier {
class tag {
 public:
  tag() = default;
  ~tag() noexcept = default;
  tag& operator=(const tag&) = delete;
  void add_object(const configuration::Tag& obj);
  void add_object(const configuration::tag& obj);
  void expand_objects(configuration::State& s);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::Tag* to_modify,
                     const configuration::Tag& new_object);
  void modify_object(const configuration::tag& obj);
  void remove_object(ssize_t idx);
  void remove_object(const configuration::tag& obj);
  void resolve_object(const configuration::Tag& obj);
  void resolve_object(const configuration::tag& obj);
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_TAG_HH
