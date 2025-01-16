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

#ifndef CENTREON_AGENT_NTDLL_HH
#define CENTREON_AGENT_NTDLL_HH

namespace com::centreon::agent {

/**As winternl.h may be included, we define our own
 * SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION */
struct M_SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION {
  LARGE_INTEGER IdleTime;
  LARGE_INTEGER KernelTime;
  LARGE_INTEGER UserTime;
  LARGE_INTEGER DpcTime;
  LARGE_INTEGER InterruptTime;
  ULONG InterruptCount;
};

void load_nt_dll();

typedef LONG(WINAPI* NtQuerySystemInformationPtr)(ULONG SystemInformationClass,
                                                  PVOID SystemInformation,
                                                  ULONG SystemInformationLength,
                                                  PULONG ReturnLength);

extern NtQuerySystemInformationPtr nt_query_system_information;

typedef NTSTATUS(NTAPI* RtlGetVersionPtr)(
    POSVERSIONINFOEXW lpVersionInformation);

extern RtlGetVersionPtr rtl_get_version;

}  // namespace com::centreon::agent

#endif
