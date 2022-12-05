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
#include "configuration/anomalydetection_helper.hh"

namespace com::centreon::engine::configuration {
anomalydetection_helper::anomalydetection_helper(Anomalydetection* obj)
    : message_helper(object_type::anomalydetection,
                     obj,
                     {
                         {"_HOST_ID", "host_id"},
                         {"_SERVICE_ID", "service_id"},
                         {"description", "service_description"},
                         {"service_groups", "servicegroups"},
                         {"contact_groups", "contactgroups"},
                         {"normal_check_interval", "check_interval"},
                         {"retry_check_interval", "retry_interval"},
                         {"active_checks_enabled", "checks_active"},
                         {"passive_checks_enabled", "checks_passive"},
                         {"severity", "severity_id"},
                     },
                     53) {
  init_anomalydetection(static_cast<Anomalydetection*>(mut_obj()));
}
}  // namespace com::centreon::engine::configuration
