/**
 * Copyright 2024 Centreon
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
#ifndef CCCM_FILE_HH
#define CCCM_FILE_HH
#include <filesystem>

namespace com::centreon::common {
std::string read_file_content(const std::filesystem::path& file_path);
std::string hash_directory(const std::filesystem::path& dir_path,
                           std::error_code& ec) noexcept;
}  // namespace com::centreon::common

#endif /* !CCCM_FILE_HH */
