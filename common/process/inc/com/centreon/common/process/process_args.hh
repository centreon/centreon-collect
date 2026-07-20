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

#ifndef CENTREON_COMMON_PROCESS_ARGS_HH
#define CENTREON_COMMON_PROCESS_ARGS_HH

namespace com::centreon::common {

namespace crypto {
class aes256;
};
/**
 * @brief The goal of this class is two store arguments in two formats
 * a vector<string> for windows boost process launcher, this vector does not
 * contain the executable
 * a vector<const char*> for spawnp, this vector contains executable and a
 * nullptr at the end
 *
 */
class process_args {
  //_c_args point to _exe_path and _args
  std::string _exe_path;
  std::vector<std::string> _args;
  std::vector<std::string> _encrypted_args;
  std::vector<const char*> _c_args;

 public:
  using pointer = std::shared_ptr<process_args>;

  process_args(const std::string_view& exe_path,
               std::vector<std::string>&& args);

  template <typename string_type>
  process_args(const std::string_view& exe_path,
               const std::initializer_list<string_type>& args)
      : _exe_path(exe_path) {
    _args.reserve(args.size());
    for (const auto& str : args) {
      _args.push_back(str);
    }
    _c_args.reserve(args.size() + 2);
    _c_args.push_back(_exe_path.c_str());
    for (const std::string& arg : _args) {
      _c_args.push_back(arg.c_str());
    }
    _c_args.push_back(nullptr);
  }

  process_args(const std::string_view& unix_commandline);
  process_args(const process_args&) = delete;
  process_args& operator=(const process_args&) = delete;

  void encrypt_args(const crypto::aes256& crypto);

  void decrypt_args(const crypto::aes256& crypto);

  void clear_unencrypted_args();

  void dump(std::string* output) const;

  const std::string& get_exe_path() const { return _exe_path; }
  const std::vector<std::string>& get_args() const { return _args; }
  const std::vector<const char*>& get_c_args() const { return _c_args; }
};

}  // namespace com::centreon::common

namespace fmt {
template <>
struct formatter<com::centreon::common::process_args> : formatter<std::string> {
  template <typename FormatContext>
  auto format(const com::centreon::common::process_args& args,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    std::string output;
    args.dump(&output);
    return formatter<std::string>::format(output, ctx);
  }
};

}  // namespace fmt

#endif
