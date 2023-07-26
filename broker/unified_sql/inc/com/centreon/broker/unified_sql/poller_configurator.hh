/*
 * Copyright 2023 Centreon
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

#ifndef CCB_UNIFIED_SQL_POLLER_CONFIGURATOR_HH
#define CCB_UNIFIED_SQL_POLLER_CONFIGURATOR_HH

#include "com/centreon/broker/namespace.hh"
CCB_BEGIN()
namespace unified_sql {
class poller_configurator {
  const std::string _path;
 public:
  poller_configurator(const std::string& path);
};
}
CCB_END()

#endif /* !CCB_UNIFIED_SQL_POLLER_CONFIGURATOR_HH */
