/**
 * Copyright 2013-2023 Centreon
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

#ifndef CCB_MISC_FILESYSTEM_HH
#define CCB_MISC_FILESYSTEM_HH

namespace com::centreon::broker::misc::filesystem {
std::list<std::string> dir_content(std::string const& path, bool recursive);
std::list<std::string> dir_content_with_filter(std::string const& path,
                                               std::string const& filter);
bool file_exists(std::string const& path);
bool dir_exists(std::string const& path);
bool mkpath(std::string const& path);
int64_t file_size(const std::string& path) noexcept;
bool writable(const std::string& name);
bool readable(const std::string& name);
}  // namespace com::centreon::broker::misc::filesystem

#endif /* CCB_MISC_FILESYSTEM_HH */
