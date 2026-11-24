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

#ifndef CCE_RETENTION_DOWNTIMES_FILE_HH
#define CCE_RETENTION_DOWNTIMES_FILE_HH

#include <filesystem>

/* In centralized configuration, Engine no longer writes retention.dat and the
 * runtime state Broker sends back does not carry the downtimes Engine owns
 * (notification_mode=engine). They are saved in a downtimes.prot file, next to
 * state.prot, and read at the next start. */
namespace com::centreon::engine::retention::downtimes_file {
bool save(const std::filesystem::path& path);
size_t restore(const std::filesystem::path& path);
}  // namespace com::centreon::engine::retention::downtimes_file

#endif  // !CCE_RETENTION_DOWNTIMES_FILE_HH
