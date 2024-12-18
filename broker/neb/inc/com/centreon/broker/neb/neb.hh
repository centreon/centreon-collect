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
#ifndef CCB_NEB_NEB_HH
#define CCB_NEB_NEB_HH

namespace com::centreon::broker::neb {
class cbmod {
  std::shared_ptr<spdlog::logger> _neb_logger;

 public:
  cbmod(const std::string& config_file);
  cbmod& operator=(const cbmod&) = delete;
};
}  // namespace com::centreon::broker::neb
#endif /* !CCB_NEB_NEB_HH */
