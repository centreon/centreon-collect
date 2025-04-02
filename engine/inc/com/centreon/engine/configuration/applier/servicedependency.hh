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
#ifndef CCE_CONFIGURATION_APPLIER_SERVICEDEPENDENCY_HH
#define CCE_CONFIGURATION_APPLIER_SERVICEDEPENDENCY_HH

#include "com/centreon/engine/configuration/applier/state.hh"
#include "common/engine_conf/indexed_state.hh"
#include "common/engine_conf/servicedependency_helper.hh"

namespace com::centreon::engine::configuration::applier {

class servicedependency {
  void _expand_services(
      const ::google::protobuf::RepeatedPtrField<std::string>& hst,
      const ::google::protobuf::RepeatedPtrField<std::string>& hg,
      const ::google::protobuf::RepeatedPtrField<std::string>& svc,
      const ::google::protobuf::RepeatedPtrField<std::string>& sg,
      configuration::indexed_state& s,
      absl::flat_hash_set<std::pair<std::string_view, std::string_view>>&
          expanded);

 public:
  servicedependency() = default;
  ~servicedependency() noexcept = default;
  servicedependency(const servicedependency&) = delete;
  servicedependency& operator=(const servicedependency&) = delete;
  void add_object(const configuration::Servicedependency& obj);
  void modify_object(configuration::Servicedependency* old_obj,
                     const configuration::Servicedependency& new_obj);
  void remove_object(uint64_t hash_key);
  void resolve_object(const configuration::Servicedependency& obj,
                      error_cnt& err);
};
}  // namespace com::centreon::engine::configuration::applier

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICEDEPENDENCY_HH
