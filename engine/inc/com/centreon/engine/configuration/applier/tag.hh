/**
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

#ifndef CCE_CONFIGURATION_APPLIER_TAG_HH
#define CCE_CONFIGURATION_APPLIER_TAG_HH
#ifdef LEGACY_CONF
#include "common/engine_legacy_conf/object.hh"
#else
#include "common/engine_conf/tag_helper.hh"
#endif

namespace com::centreon::engine::configuration {

#ifdef LEGACY_CONF
class tag;
class state;
#endif

namespace applier {
class tag {
 public:
  tag() = default;
  ~tag() noexcept = default;
  tag& operator=(const tag& other) = delete;
#ifdef LEGACY_CONF
  void add_object(const configuration::tag& obj);
  void expand_objects(configuration::state& s);
  void modify_object(const configuration::tag& obj);
  void remove_object(const configuration::tag& obj);
  void resolve_object(const configuration::tag& obj, error_cnt& err);
#else
  void add_object(const configuration::Tag& obj);
  void modify_object(configuration::Tag* to_modify,
                     const configuration::Tag& new_object);
  void remove_object(ssize_t idx);
  void resolve_object(const configuration::Tag& obj,
                      error_cnt& err);
#endif
};
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_TAG_HH
