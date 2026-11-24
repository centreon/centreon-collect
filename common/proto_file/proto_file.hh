/**
 * Copyright 2026 Centreon
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
#ifndef CCC_PROTO_FILE_HH
#define CCC_PROTO_FILE_HH

#include <google/protobuf/message.h>

#include <filesystem>
#include <system_error>

namespace com::centreon::common {

/* Why a protobuf file could not be read or written, besides the errors of the
 * system, which keep their generic category. */
enum class proto_file_errc {
  truncated_header = 1,
  unknown_format_version,
  unknown_algorithm,
  corrupted_content,
  compression_error,
  parse_error,
  serialize_error,
};

const std::error_category& proto_file_error_category() noexcept;
std::error_code make_error_code(proto_file_errc e) noexcept;

void write_proto_file(const std::filesystem::path& path,
                      const google::protobuf::Message& msg,
                      bool compress,
                      std::error_code& ec) noexcept;
void read_proto_file(const std::filesystem::path& path,
                     google::protobuf::Message& msg,
                     std::error_code& ec) noexcept;

/**
 * @brief Read a protobuf file into a new message of type T (see
 * read_proto_file(const std::filesystem::path&, google::protobuf::Message&,
 * std::error_code&)).
 *
 * @param path The file to read.
 * @param ec Cleared on success, set to the reason of the failure otherwise.
 *
 * @return The message read, or a message in an unspecified state on failure.
 */
template <typename T>
T read_proto_file(const std::filesystem::path& path, std::error_code& ec) {
  T retval;
  read_proto_file(path, retval, ec);
  return retval;
}

}  // namespace com::centreon::common

namespace std {
template <>
struct is_error_code_enum<com::centreon::common::proto_file_errc> : true_type {
};
}  // namespace std

#endif /* !CCC_PROTO_FILE_HH */
