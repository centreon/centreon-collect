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

#ifndef CCE_CONFIGURATION_STATE_GENERATED_HH
#define CCE_CONFIGURATION_STATE_GENERATED_HH

#include "state-generated.pb.h"

CCE_BEGIN()
namespace configuration {
Anomalydetection make_anomalydetection();
Command make_command();
Connector make_connector();
Contact make_contact();
Contactgroup make_contactgroup();
Host make_host();
Hostdependency make_hostdependency();
Hostescalation make_hostescalation();
Hostgroup make_hostgroup();
Hostextinfo make_hostextinfo();
Service make_service();
Servicedependency make_servicedependency();
Serviceescalation make_serviceescalation();
Servicegroup make_servicegroup();
Severity make_severity();
Tag make_tag();
Timeperiod make_timeperiod();

};  // namespace configuration
CCE_END()

#endif /* !CCE_CONFIGURATION_STATE_GENERATED_HH */
