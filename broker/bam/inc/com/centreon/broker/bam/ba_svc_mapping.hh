/*
** Copyright 2014 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_BAM_BA_SVC_MAPPING_HH
#define CCB_BAM_BA_SVC_MAPPING_HH


namespace com::centreon::broker {

namespace bam {
/**
 *  @class ba_svc_mapping ba_svc_mapping.hh
 * "com/centreon/broker/bam/ba_svc_mapping.hh"
 *  @brief Link BA ID to host name and service description.
 *
 *  Allow users to get a virtual BA host name and service description
 *  by a BA ID.
 */
class ba_svc_mapping {
  std::unordered_map<uint32_t, std::pair<std::string, std::string>> _mapping;

 public:
  ba_svc_mapping() = default;
  ba_svc_mapping(const ba_svc_mapping&) = delete;
  ~ba_svc_mapping() noexcept = default;
  ba_svc_mapping& operator=(const ba_svc_mapping& other);
  std::pair<std::string, std::string> get_service(uint32_t ba_id);
  void set(uint32_t ba_id, const std::string& hst, const std::string& svc);
};
}  // namespace bam

}

#endif  // !CCB_BAM_BA_SVC_MAPPING_HH
