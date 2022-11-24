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

namespace com {
namespace centreon {
namespace engine {
namespace configuration {
void init_anomalydetection(Anomalydetection* obj);
void init_command(Command* obj);
void init_connector(Connector* obj);
void init_contact(Contact* obj);
void init_contactgroup(Contactgroup* obj);
void init_host(Host* obj);
void init_hostdependency(Hostdependency* obj);
void init_hostescalation(Hostescalation* obj);
void init_hostgroup(Hostgroup* obj);
void init_hostextinfo(Hostextinfo* obj);
void init_service(Service* obj);
void init_servicedependency(Servicedependency* obj);
void init_serviceescalation(Serviceescalation* obj);
void init_serviceextinfo(Serviceextinfo* obj);
void init_servicegroup(Servicegroup* obj);
void init_severity(Severity* obj);
void init_tag(Tag* obj);
void init_timeperiod(Timeperiod* obj);

};  // namespace configuration
};  // namespace engine
};  // namespace centreon
};  // namespace com

#endif /* !CCE_CONFIGURATION_STATE_GENERATED_HH */
