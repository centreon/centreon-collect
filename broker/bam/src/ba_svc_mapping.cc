/*
 * Copyright 2014 Centreon
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

#include "com/centreon/broker/bam/ba_svc_mapping.hh"

using namespace com::centreon::broker::bam;

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
ba_svc_mapping& ba_svc_mapping::operator=(ba_svc_mapping const& other) {
  if (this != &other)
    _mapping = other._mapping;
  return *this;
}

/**
 *  Find a host name and service description of a BA by its ID.
 *
 *  @return A pair of host name/service description.
 */
std::pair<std::string, std::string> ba_svc_mapping::get_service(
    uint32_t ba_id) {
  auto it = _mapping.find(ba_id);
  return it != _mapping.end() ? it->second : std::make_pair("", "");
}

/**
 *  Link a BA ID to a pair of host name/service description.
 *
 *  @param[in] ba_id  BA ID.
 *  @param[in] hst    Host ID.
 *  @param[in] svc    Service ID.
 */
void ba_svc_mapping::set(uint32_t ba_id,
                         std::string const& hst,
                         std::string const& svc) {
  _mapping[ba_id] = std::make_pair(hst, svc);
}
