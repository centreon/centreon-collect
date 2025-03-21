/**
 * Copyright 2025 Centreon
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

#include "windows_util.hh"

std::string com::centreon::agent::get_last_error_as_string() {
  // Get the error message ID, if any.
  DWORD error_message_id = ::GetLastError();
  if (error_message_id == 0) {
    return std::string();  // No error message has been recorded
  }

  LPSTR message_buffer = nullptr;

  // Ask Win32 to give us the string version of that message ID.
  // The parameters we pass in, tell Win32 to create the buffer that holds the
  // message for us (because we don't yet know how long the message string will
  // be).
  size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, error_message_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPSTR)&message_buffer, 0, NULL);

  // Copy the error message into a std::string.
  std::string message(message_buffer, size);

  // Free the Win32's string's buffer.
  LocalFree(message_buffer);

  return message;
}

/**
 * @brief convert a wchar_t string to a char string
 *
 * @param lpwstr wchar_t string
 * @return std::string
 */
std::string com::centreon::agent::lpwcstr_to_acp(LPCWSTR lpwstr) {
  // get needed size
  int size_needed =
      WideCharToMultiByte(CP_ACP, 0, lpwstr, -1, NULL, 0, NULL, NULL);

  std::string utf8(size_needed, 0);

  // convert
  WideCharToMultiByte(CP_ACP, 0, lpwstr, -1, &utf8[0], size_needed, NULL, NULL);

  // remove \0
  if (!utf8.empty() && !*utf8.rbegin()) {
    utf8.resize(utf8.size() - 1);
  }
  return utf8;
}
