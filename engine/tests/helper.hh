/*
 * Copyright 2019 Centreon (https://www.centreon.com/)
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
#ifndef CENTREON_ENGINE_TESTS_HELPER_HH_
#define CENTREON_ENGINE_TESTS_HELPER_HH_

#include <filesystem>

#include "com/centreon/engine/globals.hh"

std::unique_ptr<com::centreon::engine::configuration::state_helper>
init_config_state(void);
void deinit_config_state(void);

/**
 * @brief The build directory, where the tests' fixtures (*.cfg, helper
 * binaries...) live: the parent of the directory holding this test binary
 * (<build>/tests). Usable at static initialization, unlike the cwd main()
 * moves to. Falls back to the cwd when /proc/self/exe cannot be read.
 */
inline std::filesystem::path build_dir() {
  std::error_code ec;
  auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (ec)
    return std::filesystem::current_path();
  return exe.parent_path().parent_path();
}

#endif  // CENTREON_ENGINE_TESTS_HELPER_HH_
