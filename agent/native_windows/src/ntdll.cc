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

#include "ntdll.hh"

namespace com::centreon::agent {

// ntdll.dll handle
static HMODULE _ntdll = nullptr;

NtQuerySystemInformationPtr nt_query_system_information = nullptr;
RtlGetVersionPtr rtl_get_version = nullptr;

/**
 * @brief load ntdll.dll and get NtQuerySystemInformation and RtlGetVersion
 * address
 *
 */
void load_nt_dll() {
  if (!_ntdll) {
    _ntdll = LoadLibraryA("ntdll.dll");
    if (!_ntdll) {
      throw std::runtime_error("Failed to load ntdll.dll");
    }
  }

  // get NtQuerySystemInformation Pointer
  nt_query_system_information = (NtQuerySystemInformationPtr)GetProcAddress(
      _ntdll, "NtQuerySystemInformation");
  if (!nt_query_system_information) {
    FreeLibrary(_ntdll);
    _ntdll = nullptr;
    throw std::runtime_error(
        "Failed to get address of NtQuerySystemInformation");
  }

  rtl_get_version = (RtlGetVersionPtr)GetProcAddress(_ntdll, "RtlGetVersion");
  if (!rtl_get_version) {
    FreeLibrary(_ntdll);
    _ntdll = nullptr;
    throw std::runtime_error("Failed to get address of RtlGetVersion");
  }
}
}  // namespace com::centreon::agent