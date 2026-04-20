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

#ifndef CCCP_SCRIPT_CHILD_HH
#define CCCP_SCRIPT_CHILD_HH

#include <boost/asio/system_timer.hpp>
#include "com/centreon/common/process/fork.hh"
#include "com/centreon/connector/perl/endpoint.hh"

struct STRUCT_SV;

namespace com::centreon::connector::perl {
class script_child : public com::centreon::common::fork<false> {
  const std::string _script_path;
  const std::string _additional_code;
  std::filesystem::file_time_type _check_script_mtime;
  STRUCT_SV* _check_script_handle = nullptr;
  std::unique_ptr<endpoint> _endpoint;
  std::string _global_error;
  asio::system_timer _minute_timer;

  int _run(int stdin_fd, int stdout_fd, int stderr_fd) override;

  void _compile_script(const std::string& loader_path);
  void _load_check_script();
  std::string _write_loader_to_disk(const std::string_view& additional_code);

 public:
  script_child(const std::shared_ptr<asio::io_context> io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::string& script_path,
               const std::string& additional_code);

  script_child(const script_child&) = delete;
  script_child& operator=(const script_child&) = delete;

  ~script_child();
};
}  // namespace com::centreon::connector::perl
#endif
