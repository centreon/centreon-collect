/**
 * Copyright 2024 Centreon
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

#ifndef CCEC_LOGGING_HH
#define CCEC_LOGGING_HH

/**
 * This namespace is based on the com/centreon/engine/logging/logger.hh file.
 *
 * It contains the minimum needed by the configuration module about logging so
 * that the legacy logger can still be configured.
 *
 * FIXME: We hope this legacy logger can be removed very soon.
 */
namespace com::centreon::engine::configuration::logging {
enum type_value {
  log_all = 2096895ull,
  dbg_all = 4095ull << 32,
  all = log_all | dbg_all,
};

enum verbosity_level { basic = 0u, more = 1u, most = 2u };

}  // namespace com::centreon::engine::configuration::logging

#endif /* !CCEC_LOGGING_HH */
