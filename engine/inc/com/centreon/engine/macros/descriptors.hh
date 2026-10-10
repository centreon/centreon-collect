/**
 * Copyright 2026 Centreon
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

#ifndef CCE_MACROS_DESCRIPTORS_HH
#define CCE_MACROS_DESCRIPTORS_HH

#include <absl/types/span.h>

#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>

class nagios_macros;

namespace com::centreon::common::timeperiods {
class timeperiod;
}

namespace com::centreon::engine {
class contact;
class contactgroup;
class host;
class hostgroup;
class service;
class servicegroup;

namespace macros {

/**
 * @brief The object family a macro is read from. It selects the handler
 * used to compute the macro value.
 */
enum class family : uint8_t {
  host,
  hostgroup,
  service,
  servicegroup,
  contact,
  contactgroup,
  notification,
  datetime,
  statics,
  summary,
};

/**
 * @brief The objects a macro is computed on. Only the one matching the
 * family of the macro is set: hst for a host macro, svc for a service
 * macro... The date/time macros use tp and when.
 */
struct macro_target {
  host* hst = nullptr;
  service* svc = nullptr;
  contact* cntct = nullptr;
  hostgroup* hg = nullptr;
  servicegroup* sg = nullptr;
  contactgroup* cg = nullptr;
  common::timeperiods::timeperiod* tp = nullptr;
  time_t when = 0;
};

/* Computes the value of a macro on its target. */
using getter = std::string (*)(const macro_target& target, nagios_macros* mac);

/**
 * @brief Description of a macro stored in nagios_macros::x[].
 */
struct descriptor {
  /* The macro id, i.e. its index in nagios_macros::x[] (MACRO_HOSTNAME...). */
  uint32_t id;
  /* The macro name without its dollars ("HOSTNAME"...). */
  std::string_view name;
  family fam;
  /* True if the macro value must always be stripped of illegal characters
   * and escaped. */
  bool cleaned;
  /* Computes the value of the macro. */
  getter get;
};

const descriptor* find_descriptor(std::string_view name);
absl::Span<const descriptor> all_descriptors();

}  // namespace macros

}  // namespace com::centreon::engine

#endif  // !CCE_MACROS_DESCRIPTORS_HH
